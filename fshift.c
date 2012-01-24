/*
 *	Quadrature frequency shifter.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#include <math.h>
#include "gstiq.h"
#include <string.h>

static GstElementDetails iqfshift_details = GST_ELEMENT_DETAILS(
	"Quadrature frequency shift plugin",
	"Filter/Effect/Audio",
	"Frequency shifts a Quadrature signal",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0,
	ARG_SHIFT
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-complex-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "		/* two floats == 2 * 32 */
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 1"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-complex-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "		/* two floats == 2 * 32 */
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_iqfshift_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqfshift *fshift;
	GstBuffer *outbuf;
	GstCaps *caps;
	gfloat *iqbuf, *iqbufout, ival, qval, cosine, sine;
	int i;

	fshift = GST_IQFSHIFT(gst_pad_get_parent(pad));

	caps = gst_pad_get_caps(fshift->srcpad);
	if (fshift->shift != 0.0) {
		if (!gst_buffer_is_writable(buf)) {
			outbuf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf));
			GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
			GST_BUFFER_OFFSET(outbuf) = GST_BUFFER_OFFSET(buf);
		} else {
			outbuf = buf;
		}

		iqbuf = (gfloat *)GST_BUFFER_DATA(buf);
		iqbufout = (gfloat *)GST_BUFFER_DATA(outbuf);
		for (i = 0; i < GST_BUFFER_SIZE(outbuf)/sizeof(gfloat); i += 2) {
			ival = iqbuf[i];
			qval = iqbuf[i+1];
			sine = sin(fshift->angle);
			cosine = cos(fshift->angle);
			iqbufout[i] = sine * ival + cosine * qval;
			iqbufout[i+1] = sine * qval - cosine * ival;
			fshift->angle += fshift->step;
			if (fshift->angle > 2 * M_PI)
				fshift->angle -= 2 * M_PI;
			if (fshift->angle < -2 * M_PI)
				fshift->angle += 2 * M_PI;
		}
		if (!gst_buffer_is_writable(buf))
			gst_buffer_unref(buf);
		gst_buffer_set_caps(outbuf, caps);
		gst_pad_push(fshift->srcpad, outbuf);
	} else {
		gst_buffer_set_caps(buf, caps);
		gst_pad_push(fshift->srcpad, buf);
	}
	gst_caps_unref(caps);
	return GST_FLOW_OK;
}

static void gst_iqfshift_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_iqfshift *fshift;

	g_return_if_fail(GST_IS_IQFSHIFT(object));
	fshift = GST_IQFSHIFT(object);

	switch(prop_id) {
		case ARG_SHIFT:
			fshift->shift = g_value_get_float(value);
			fshift->step =
			    fshift->shift * 2 * M_PI / (float)fshift->rate;
			break;
		default:
			break;
	}
}

static void gst_iqfshift_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_iqfshift *fshift;

	g_return_if_fail(GST_IS_IQFSHIFT(object));
	fshift = GST_IQFSHIFT(object);

	switch(prop_id) {
		case ARG_SHIFT:
			g_value_set_float(value, fshift->shift);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_iqfshift_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqfshift_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_iqfshift *fshift;

	fshift = GST_IQFSHIFT(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &fshift->rate);

	fshift->step =
	    fshift->shift * 2 * M_PI / (float)fshift->rate;

	gst_pad_use_fixed_caps(
	    pad == fshift->srcpad ? fshift->sinkpad : fshift->srcpad);
	return gst_pad_set_caps(
	    pad == fshift->srcpad ? fshift->sinkpad : fshift->srcpad,
	    gst_caps_copy(caps));
}

static void gst_iqfshift_class_init(Gst_iqfshift_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_iqfshift_set_property;
	gobject_class->get_property = gst_iqfshift_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_SHIFT,
	    g_param_spec_float("shift", "shift", "shift", -G_MAXFLOAT, G_MAXFLOAT, 0.0, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_iqfshift_change_state;

	gst_element_class_set_details(gstelement_class, &iqfshift_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqfshift_init(Gst_iqfshift *fshift)
{
	fshift->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (fshift->sinkpad, gst_iqfshift_chain);
	gst_element_add_pad (GST_ELEMENT(fshift), fshift->sinkpad);

	fshift->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(fshift), fshift->srcpad);

	gst_pad_set_setcaps_function(fshift->srcpad, gst_iqfshift_setcaps);
	gst_pad_set_setcaps_function(fshift->sinkpad, gst_iqfshift_setcaps);

	fshift->shift = 0.0;
	fshift->angle = 0.0;
}

GType gst_iqfshift_get_type(void)
{
	static GType iqfshift_type = 0;

	if (!iqfshift_type) {
		static const GTypeInfo iqfshift_info = {
			sizeof(Gst_iqfshift_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqfshift_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqfshift),
			0,
			(GInstanceInitFunc)gst_iqfshift_init,
		};
		iqfshift_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQFshift", &iqfshift_info, 0);
	}
	return iqfshift_type;
}
