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

static GstElementDetails iqfmdem_details = GST_ELEMENT_DETAILS(
	"Quadrature frequency demodulator plugin",
	"Filter/Effect/Audio",
	"Frequency demodulates a Quadrature signal",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0,
	ARG_DEVIATION
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
		"buffer-frames = [ 0, MAX ], "
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
		"buffer-frames = [ 0, MAX ], "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_iqfmdem_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqfmdem *fmdem;
	GstBuffer *outbuf;
	GstCaps *caps;
	gfloat *iqbuf, *bufout, angle, dev;
	int i;

	fmdem = GST_IQFMDEM(gst_pad_get_parent(pad));

	outbuf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf)/2);
	GST_BUFFER_OFFSET(outbuf) = fmdem->offset;
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);

	iqbuf = (gfloat *)GST_BUFFER_DATA(buf);
	bufout = (gfloat *)GST_BUFFER_DATA(outbuf);
	for (i = 0; i < GST_BUFFER_SIZE(outbuf)/sizeof(gfloat); i ++) {
		angle = iqbuf[i*2 + 1];
		dev = angle - fmdem->prevangle;
		fmdem->prevangle = angle;
		if (dev > M_PI)
			dev -= 2 * M_PI;
		if (dev < -M_PI)
			dev += 2 * M_PI;
		bufout[i] = dev * fmdem->normal;
		fmdem->avrg += (bufout[i] - fmdem->avrg) / fmdem->avrglen;
		bufout[i] -= fmdem->avrg;
		fmdem->filter += (bufout[i] - fmdem->filter)/(10);
		bufout[i] = fmdem->filter;
		
	}
	fmdem->offset += i;

	caps = gst_pad_get_caps(fmdem->srcpad);
	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);
	gst_buffer_unref(buf);
	gst_pad_push(fmdem->srcpad, outbuf);
	return GST_FLOW_OK;
}

static void gst_iqfmdem_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_iqfmdem *fmdem;

	g_return_if_fail(GST_IS_IQFMDEM(object));
	fmdem = GST_IQFMDEM(object);

	switch(prop_id) {
		case ARG_DEVIATION:
			fmdem->deviation = g_value_get_float(value);
			fmdem->normal = fmdem->deviation / (float)fmdem->rate;
			break;
		default:
			break;
	}
}

static void gst_iqfmdem_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_iqfmdem *fmdem;

	g_return_if_fail(GST_IS_IQFMDEM(object));
	fmdem = GST_IQFMDEM(object);

	switch(prop_id) {
		case ARG_DEVIATION:
			g_value_set_float(value, fmdem->deviation);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_iqfmdem_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqfmdem_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_iqfmdem *fmdem;
	GstCaps *newcaps;
	GstPad *other;
	gint bufferframes;

	fmdem = GST_IQFMDEM(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &fmdem->rate);
	gst_structure_get_int(structure, "buffer-frames", &bufferframes);

	fmdem->normal = (float)fmdem->rate / (fmdem->deviation * M_PI * 2);
	fmdem->avrglen = fmdem->rate / 10;

	other = pad == fmdem->srcpad ? fmdem->sinkpad : fmdem->srcpad;
	newcaps = gst_pad_get_caps(other);
	newcaps = gst_caps_make_writable(newcaps);
	structure = gst_caps_get_structure(newcaps, 0);
	gst_structure_set(structure, "rate", G_TYPE_INT, fmdem->rate, NULL);
	gst_structure_set(structure, "buffer-frames", G_TYPE_INT, bufferframes,
	    NULL);

	gst_pad_use_fixed_caps(other);
	return gst_pad_set_caps(other, newcaps);
}

static void gst_iqfmdem_class_init(Gst_iqfmdem_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_iqfmdem_set_property;
	gobject_class->get_property = gst_iqfmdem_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_DEVIATION,
	    g_param_spec_float("deviation", "deviation", "deviation",
	    1.0, G_MAXFLOAT, 1500.0, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_iqfmdem_change_state;

	gst_element_class_set_details(gstelement_class, &iqfmdem_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqfmdem_init(Gst_iqfmdem *fmdem)
{
	fmdem->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (fmdem->sinkpad, gst_iqfmdem_chain);
	gst_element_add_pad (GST_ELEMENT(fmdem), fmdem->sinkpad);

	fmdem->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(fmdem), fmdem->srcpad);

	gst_pad_set_setcaps_function(fmdem->srcpad, gst_iqfmdem_setcaps);
	gst_pad_set_setcaps_function(fmdem->sinkpad, gst_iqfmdem_setcaps);

	fmdem->deviation = 1500.0;
	fmdem->avrg = 0.0;
	fmdem->filter = 0.0;
	fmdem->offset = 0;
}

GType gst_iqfmdem_get_type(void)
{
	static GType iqfmdem_type = 0;

	if (!iqfmdem_type) {
		static const GTypeInfo iqfmdem_info = {
			sizeof(Gst_iqfmdem_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqfmdem_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqfmdem),
			0,
			(GInstanceInitFunc)gst_iqfmdem_init,
		};
		iqfmdem_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQFMDem", &iqfmdem_info, 0);
	}
	return iqfmdem_type;
}
