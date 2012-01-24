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

static GstElementDetails iqpolarhp_details = GST_ELEMENT_DETAILS(
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
		"audio/x-polar-float, "
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
		"audio/x-polar-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "		/* two floats == 2 * 32 */
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_iqpolarhp_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqpolarhp *polarhp;
	GstBuffer *outbuf;
	GstCaps *caps;
	gfloat *iqbuf, *iqbufout, ival, qval, cosine, sine, anglevel;
	int i;

	polarhp = GST_IQPOLARHP(gst_pad_get_parent(pad));

	caps = gst_pad_get_caps(polarhp->srcpad);
//	if (polarhp->shift != 0.0) {
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

			anglevel = qval - polarhp->angle;
			polarhp->angle = qval;
			if (anglevel > M_PI)
				anglevel -= 2 * M_PI;
			if (anglevel < -M_PI)
				anglevel += 2 * M_PI;
			polarhp->step *= 0.9999;
			polarhp->step += anglevel * 0.0001;

			polarhp->corangle += polarhp->step * 0.001;
			if (polarhp->corangle > M_PI)
				polarhp->corangle -= M_PI * 2;
			if (polarhp->corangle < -M_PI)
				polarhp->corangle += M_PI * 2;

			iqbufout[i]= ival;
			iqbufout[i+1] = qval - polarhp->corangle;
			if (iqbufout[i+1] > M_PI)
				iqbufout[i+1] -= 2 * M_PI;
			if (iqbufout[i+1] < -M_PI)
				iqbufout[i+1] += 2 * M_PI;
		}
		if (buf != outbuf)
			gst_buffer_unref(buf);
		gst_buffer_set_caps(outbuf, caps);
		gst_pad_push(polarhp->srcpad, outbuf);
//	} else {
//		gst_buffer_set_caps(buf, caps);
//		gst_pad_push(polarhp->srcpad, buf);
//	}
	gst_caps_unref(caps);
	gst_object_unref(polarhp);
	return GST_FLOW_OK;
}

static void gst_iqpolarhp_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_iqpolarhp *polarhp;

	g_return_if_fail(GST_IS_IQPOLARHP(object));
	polarhp = GST_IQPOLARHP(object);

	switch(prop_id) {
		case ARG_SHIFT:
			polarhp->shift = g_value_get_float(value);
			polarhp->step =
			    polarhp->shift * 2 * M_PI / (float)polarhp->rate;
			break;
		default:
			break;
	}
}

static void gst_iqpolarhp_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_iqpolarhp *polarhp;

	g_return_if_fail(GST_IS_IQPOLARHP(object));
	polarhp = GST_IQPOLARHP(object);

	switch(prop_id) {
		case ARG_SHIFT:
			g_value_set_float(value, polarhp->shift);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_iqpolarhp_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqpolarhp_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_iqpolarhp *polarhp;
	gboolean ret;

	polarhp = GST_IQPOLARHP(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &polarhp->rate);

	polarhp->step =
	    polarhp->shift * 2 * M_PI / (float)polarhp->rate;

	gst_pad_use_fixed_caps(
	    pad == polarhp->srcpad ? polarhp->sinkpad : polarhp->srcpad);
	ret = gst_pad_set_caps(
	    pad == polarhp->srcpad ? polarhp->sinkpad : polarhp->srcpad,
	    gst_caps_copy(caps));
	gst_object_unref(polarhp);
	return ret;
}

static void gst_iqpolarhp_class_init(Gst_iqpolarhp_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_iqpolarhp_set_property;
	gobject_class->get_property = gst_iqpolarhp_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_SHIFT,
	    g_param_spec_float("shift", "shift", "shift", -G_MAXFLOAT, G_MAXFLOAT, 0.0, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_iqpolarhp_change_state;

	gst_element_class_set_details(gstelement_class, &iqpolarhp_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqpolarhp_init(Gst_iqpolarhp *polarhp)
{
	polarhp->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (polarhp->sinkpad, gst_iqpolarhp_chain);
	gst_element_add_pad (GST_ELEMENT(polarhp), polarhp->sinkpad);

	polarhp->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(polarhp), polarhp->srcpad);

	gst_pad_set_setcaps_function(polarhp->srcpad, gst_iqpolarhp_setcaps);
	gst_pad_set_setcaps_function(polarhp->sinkpad, gst_iqpolarhp_setcaps);

	polarhp->shift = 0.0;
	polarhp->angle = 0.0;
}

GType gst_iqpolarhp_get_type(void)
{
	static GType iqpolarhp_type = 0;

	if (!iqpolarhp_type) {
		static const GTypeInfo iqpolarhp_info = {
			sizeof(Gst_iqpolarhp_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqpolarhp_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqpolarhp),
			0,
			(GInstanceInitFunc)gst_iqpolarhp_init,
		};
		iqpolarhp_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQPolarHP", &iqpolarhp_info, 0);
	}
	return iqpolarhp_type;
}
