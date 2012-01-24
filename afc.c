/*
 *	Use FFT results to determine center frequency.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#include "gstiq.h"
#include <string.h>

static GstElementDetails afc_details = GST_ELEMENT_DETAILS(
	"AFC plugin",
	"Filter/Effect/Audio",
	"Use FFT data for Automatic Frequency Control",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0,
	ARG_AFC,
	ARG_MIRROR,
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-fft-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "		/* complex float = 2 * 32 */
		"rate = (int) [ 1, MAX ], "
		"length = (int) [ 1, MAX ], "
		"channels = (int) 1"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_afc_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_afc *afc;
	GstCaps *caps;
	int i;
	float tafc;
	float step;
	float max = 0.0;
	float f;
	float integrate, sum;
	GstBuffer *bufout;

	afc = GST_AFC(gst_pad_get_parent(pad));
	if (afc->length == -1) {
		GstStructure *structure;
		caps = gst_pad_get_caps(afc->sinkpad);
		structure = gst_caps_get_structure(caps, 0);
		gst_structure_get_int(structure, "length", &afc->length);
		gst_structure_get_int(structure, "rate", &afc->rate);
		gst_caps_unref(caps);
	}

	integrate = 0.0;
	sum = 0.0;
	step = (float)afc->rate / (float)afc->length;
	max = 0.0;
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(float); i++) {
		if (((float*)GST_BUFFER_DATA(buf))[i] > max)
			max = ((float*)GST_BUFFER_DATA(buf))[i];
		if (-((float*)GST_BUFFER_DATA(buf))[i] > max)
			max = -((float*)GST_BUFFER_DATA(buf))[i];
	}
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(float)/4; i++) {
		f = hypot(
		    ((float*)GST_BUFFER_DATA(buf))[i*2],
		    ((float*)GST_BUFFER_DATA(buf))[i*2+1]);
		if (f < 0.5 * max)
			f = 0;
		else
			f = max;
		integrate += f * (float)i;
		sum += f;
	}
	if (!afc->mirror) for (; i < GST_BUFFER_SIZE(buf)/sizeof(float)/2; i++){
		f = hypot(
		    ((float*)GST_BUFFER_DATA(buf))[i*2],
		    ((float*)GST_BUFFER_DATA(buf))[i*2+1]);
		if (f < 0.1 * max)
			f = 0;
		integrate += f * (float)(i - afc->length);
		sum += f;
	}
	f = integrate / sum * step - afc->afc;
	i = integrate / sum + 0.5;
	
	if (f < 0.0)
		f = -f;
	{
		float divf = afc->lastf - f;

		afc->lastf = f;
		if (divf < 0.0)
			divf = -divf;
		f *= f / divf;// * f / divf;
	}
	f /= 10000000 / afc->length;
	if (f >= 1.0)
		f = 0.9;
	f *= f;
	tafc = afc->afc * (1-f) + integrate / sum * step * f;
	if (!isnan(tafc))
		afc->afc = tafc;

	bufout = gst_buffer_new_and_alloc(sizeof(float));
	GST_BUFFER_SIZE(bufout) = sizeof(float);
	*(float*)GST_BUFFER_DATA(bufout) = afc->afc;
	GST_BUFFER_OFFSET(bufout) = afc->offset++;
	GST_BUFFER_TIMESTAMP(bufout) = GST_BUFFER_TIMESTAMP(buf);
	caps = gst_pad_get_caps(afc->srcpad);
	gst_buffer_set_caps(bufout, caps);
	gst_caps_unref(caps);
	gst_buffer_unref(buf);
	gst_pad_push(afc->srcpad, bufout);

	gst_object_unref(afc);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_afc_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_afc_setcaps(GstPad *pad, GstCaps *caps)
{
	return TRUE;
}

static void gst_afc_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_afc *afc;

	g_return_if_fail(GST_IS_AFC(object));
	afc = GST_AFC(object);

	switch(prop_id) {
		case ARG_MIRROR:
			afc->mirror = g_value_get_int(value);
			break;
		default:
			break;
	}
}

static void gst_afc_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_afc *afc;

	g_return_if_fail(GST_IS_AFC(object));
	afc = GST_AFC(object);

	switch(prop_id) {
		case ARG_MIRROR:
			g_value_set_int(value, afc->mirror);
			break;
		case ARG_AFC:
			g_value_set_float(value, afc->afc);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void gst_afc_class_init(Gst_afc_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_afc_set_property;
	gobject_class->get_property = gst_afc_get_property;

	g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_MIRROR,
	    g_param_spec_int("mirror", "mirror", "mirror", 0, 1, 0,
	    G_PARAM_READWRITE));
	g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_AFC,
	    g_param_spec_float("afc", "afc", "afc", -G_MAXFLOAT, G_MAXFLOAT, 0.0,
	    G_PARAM_READWRITE));

	gstelement_class->change_state = gst_afc_change_state;

	gst_element_class_set_details(gstelement_class, &afc_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class, 
	    gst_static_pad_template_get(&src_template));
}

static void gst_afc_init(Gst_afc *afc)
{
	afc->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (afc->sinkpad, gst_afc_chain);
	gst_element_add_pad(GST_ELEMENT(afc), afc->sinkpad);

	afc->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(afc), afc->srcpad);

	gst_pad_set_setcaps_function(afc->sinkpad, gst_afc_setcaps);
	gst_pad_set_setcaps_function(afc->srcpad, gst_afc_setcaps);
	gst_pad_use_fixed_caps(afc->sinkpad);
	gst_pad_use_fixed_caps(afc->srcpad);

	afc->length = -1;
	afc->rate = 44100;
	afc->afc = 0.0;
	afc->mirror = 1;
	afc->offset = 0;
	afc->lastf = 0.0;
}

GType gst_afc_get_type(void)
{
	static GType afc_type = 0;

	if (!afc_type) {
		static const GTypeInfo afc_info = {
			sizeof(Gst_afc_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_afc_class_init,
			NULL,
			NULL,
			sizeof(Gst_afc),
			0,
			(GInstanceInitFunc)gst_afc_init,
		};
		afc_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstAFC", &afc_info, 0);
	}
	return afc_type;
}
