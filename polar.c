/*
 *	Quadrature vector to polar conversion filter.
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
#include <math.h>

static GstElementDetails iqpolar_details = GST_ELEMENT_DETAILS(
	"Quadrature Vector to Polar conversion",
	"Filter/Effect/Audio",
	"Converts a vector Quadrature signal to polar representation",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-complex-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"width = (int) 64, "
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
		"depth = (int) 64, "
		"width = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_iqpolar_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqpolar *polar;
	GstCaps *caps;
	GstBuffer *outbuf;
	gfloat *iqbufout, *iqbuf, ival, qval;
	int i;

	polar = GST_IQPOLAR(gst_pad_get_parent(pad));

	if (!gst_buffer_is_writable(buf)) {
		outbuf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf));
		GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
		GST_BUFFER_OFFSET(outbuf) = GST_BUFFER_OFFSET(buf);
	} else
		outbuf = buf;
	iqbuf = (gfloat *)GST_BUFFER_DATA(buf);
	iqbufout = (gfloat *)GST_BUFFER_DATA(outbuf);
	for (i = 0; i < GST_BUFFER_SIZE(outbuf)/sizeof(gfloat); i += 2) {
		ival = iqbuf[i];
		qval = iqbuf[i+1];
		if (qval == 0.0) {
			iqbufout[i+1] = copysign(M_PI_2, ival);
		} else {
			iqbufout[i+1] = atan(ival/qval);
			if (qval < 0.0) {
				iqbufout[i+1] += copysign(M_PI, ival);
			}
		}
		iqbufout[i] = hypot(ival, qval);
	}
	if (!gst_buffer_is_writable(buf))
		gst_buffer_unref(buf);
	caps = gst_pad_get_caps(polar->srcpad);
	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);
	gst_pad_push(polar->srcpad, outbuf);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_iqpolar_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqpolar_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_iqpolar *polar;
	GstCaps *newcaps;
	GstStructure *structure;
	GstPad *other;
	gint rate;

	polar = GST_IQPOLAR(gst_pad_get_parent(pad));
	other = pad == polar->srcpad ? polar->sinkpad : polar->srcpad;
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &rate);
	newcaps = gst_pad_get_caps(other);
	newcaps = gst_caps_make_writable(newcaps);
	structure = gst_caps_get_structure(newcaps, 0);
	gst_structure_set(structure, "rate", G_TYPE_INT, rate, NULL);
	
	gst_pad_use_fixed_caps(other);
	return gst_pad_set_caps(other, newcaps);
}

static void gst_iqpolar_class_init(Gst_iqpolar_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_iqpolar_change_state;

	gst_element_class_set_details(gstelement_class, &iqpolar_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqpolar_init(Gst_iqpolar *polar)
{
	polar->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (polar->sinkpad, gst_iqpolar_chain);
	gst_element_add_pad (GST_ELEMENT(polar), polar->sinkpad);

	polar->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(polar), polar->srcpad);

	gst_pad_set_setcaps_function(polar->srcpad, gst_iqpolar_setcaps);
	gst_pad_set_setcaps_function(polar->sinkpad, gst_iqpolar_setcaps);
}

GType gst_iqpolar_get_type(void)
{
	static GType iqpolar_type = 0;

	if (!iqpolar_type) {
		static const GTypeInfo iqpolar_info = {
			sizeof(Gst_iqpolar_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqpolar_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqpolar),
			0,
			(GInstanceInitFunc)gst_iqpolar_init,
		};
		iqpolar_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQPolar", &iqpolar_info, 0);
	}
	return iqpolar_type;
}
