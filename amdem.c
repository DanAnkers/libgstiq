/*
 *	Quadrature frequency demodulator.
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

static GstElementDetails iqamdem_details = GST_ELEMENT_DETAILS(
	"Quadrature amplitude demodulator plugin",
	"Filter/Effect/Audio",
	"Amplitude demodulates a Quadrature signal",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0,
	ARG_DEPTH
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-polar-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"width = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"buffer-frames = (int) [ 0, MAX ], "
		"channels = (int) 1"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"width = (int) 32, "
		"rate = (int) [ 1, MAX ], "
		"buffer-frames = (int) [0, MAX ], "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_iqamdem_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqamdem *amdem;
	GstBuffer *outbuf;
	GstCaps *caps;
	gfloat *iqbuf, *bufout, abs;
	int i;

	amdem = GST_IQAMDEM(gst_pad_get_parent(pad));

	outbuf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf)/2);
	GST_BUFFER_OFFSET(outbuf) = amdem->offset;
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);

	iqbuf = (gfloat *)GST_BUFFER_DATA(buf);
	bufout = (gfloat *)GST_BUFFER_DATA(outbuf);
	for (i = 0; i < GST_BUFFER_SIZE(outbuf)/sizeof(gfloat); i++) {
		abs = iqbuf[i*2];
		bufout[i] = (abs - amdem->avrg) / amdem->depth;
		amdem->avrg += (abs - amdem->avrg) / amdem->avrglen;
	}
	amdem->offset += i;

	caps = gst_pad_get_caps(amdem->srcpad);
	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);
	gst_buffer_unref(buf);
	gst_pad_push(amdem->srcpad, outbuf);
	return GST_FLOW_OK;
}

static void gst_iqamdem_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_iqamdem *amdem;

	g_return_if_fail(GST_IS_IQAMDEM(object));
	amdem = GST_IQAMDEM(object);

	switch(prop_id) {
		case ARG_DEPTH:
			amdem->depth = g_value_get_float(value);
			break;
		default:
			break;
	}
}

static void gst_iqamdem_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_iqamdem *amdem;

	g_return_if_fail(GST_IS_IQAMDEM(object));
	amdem = GST_IQAMDEM(object);

	switch(prop_id) {
		case ARG_DEPTH:
			g_value_set_float(value, amdem->depth);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_iqamdem_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqamdem_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_iqamdem *amdem;
	GstCaps *newcaps;
	GstPad *other;
	gint bufferframes;

	amdem = GST_IQAMDEM(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &amdem->rate);
	gst_structure_get_int(structure, "buffer-frames", &bufferframes);

	amdem->avrglen = amdem->rate / 10;

	other = pad == amdem->srcpad ? amdem->sinkpad : amdem->srcpad;
	newcaps = gst_pad_get_caps(other);
	newcaps = gst_caps_make_writable(newcaps);
	structure = gst_caps_get_structure(newcaps, 0);
	gst_structure_set(structure, "rate", G_TYPE_INT, amdem->rate, NULL);
	gst_structure_set(structure, "buffer-frames", G_TYPE_INT, bufferframes,
	    NULL);

	gst_pad_use_fixed_caps(other);
	return gst_pad_set_caps(other, newcaps);
}

static void gst_iqamdem_class_init(Gst_iqamdem_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_iqamdem_set_property;
	gobject_class->get_property = gst_iqamdem_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_DEPTH,
	    g_param_spec_float("depth", "depth", "depth",
	    0.01, G_MAXFLOAT, 1.0, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_iqamdem_change_state;

	gst_element_class_set_details(gstelement_class, &iqamdem_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqamdem_init(Gst_iqamdem *amdem)
{
	amdem->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function(amdem->sinkpad, gst_iqamdem_chain);
	gst_element_add_pad(GST_ELEMENT(amdem), amdem->sinkpad);

	amdem->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(amdem), amdem->srcpad);

	gst_pad_set_setcaps_function(amdem->srcpad, gst_iqamdem_setcaps);
	gst_pad_set_setcaps_function(amdem->sinkpad, gst_iqamdem_setcaps);

	amdem->depth = 1.0;
	amdem->avrg = 0.0;
	amdem->offset = 0;
}

GType gst_iqamdem_get_type(void)
{
	static GType iqamdem_type = 0;

	if (!iqamdem_type) {
		static const GTypeInfo iqamdem_info = {
			sizeof(Gst_iqamdem_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqamdem_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqamdem),
			0,
			(GInstanceInitFunc)gst_iqamdem_init,
		};
		iqamdem_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQAMDem", &iqamdem_info, 0);
	}
	return iqamdem_type;
}
