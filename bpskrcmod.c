/*
 *	Quadrature BPSK raised cosine modulator.
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

static GstElementDetails bpskrcmod_details = GST_ELEMENT_DETAILS(
	"Quadrature BPSK-RC modulator plugin",
	"Filter/Effect/Audio",
	"BPSK raised cosine modulator with quadrature output",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
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
		"depth = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"buffer-frames = (int) 0, "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_bpskrcmod_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_bpskrcmod *bpskrcmod;
	GstBuffer *outbuf;
	GstCaps *caps;
	gfloat *iqbuf, *bufin;
	int i, j;

	bpskrcmod = GST_BPSKRCMOD(gst_pad_get_parent(pad));

	caps = gst_pad_get_caps(bpskrcmod->srcpad);
	if (!gst_caps_is_fixed(caps) || bpskrcmod->symbollen < 0) {
		GstStructure *structure;
		gst_caps_unref(caps);
		caps = gst_pad_get_allowed_caps(bpskrcmod->srcpad);
		structure = gst_caps_get_structure(caps, 0);
		gst_structure_get_int(structure, "rate", &bpskrcmod->rate);
		bpskrcmod->symbollen =
		    (float)bpskrcmod->rate/(float)bpskrcmod->symbolrate;
		gst_pad_use_fixed_caps(bpskrcmod->srcpad);
	}

	outbuf = gst_buffer_new_and_alloc(
	    GST_BUFFER_SIZE(buf) * bpskrcmod->symbollen*2);
	GST_BUFFER_OFFSET(outbuf) = GST_BUFFER_OFFSET(buf);
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);

	bufin = (gfloat *)GST_BUFFER_DATA(buf);
	iqbuf = (gfloat *)GST_BUFFER_DATA(outbuf);
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(gfloat); i ++) {
		for (j = 0; j < bpskrcmod->symbollen; j++) {
			iqbuf[(i*bpskrcmod->symbollen+j)*2] = (
			    bufin[i] * -0.5 *
			    (cos((j * M_PI) / bpskrcmod->symbollen) - 1) ) + (
			    bpskrcmod->lastsymbol * -0.499 *
			    (cos((j * M_PI) / bpskrcmod->symbollen + M_PI) - 1));
			iqbuf[(i*bpskrcmod->symbollen+j)*2 + 1] = 0.0;
		}
		bpskrcmod->lastsymbol = bufin[i];
	}

	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);
	gst_buffer_unref(buf);
	gst_pad_push(bpskrcmod->srcpad, outbuf);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_bpskrcmod_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static void gst_bpskrcmod_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
}

static void gst_bpskrcmod_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	switch(prop_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static gboolean gst_bpskrcmod_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_bpskrcmod *bpskrcmod;

	bpskrcmod = GST_BPSKRCMOD(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &bpskrcmod->symbolrate);

	return TRUE;
}

static void gst_bpskrcmod_class_init(Gst_bpskrcmod_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_bpskrcmod_change_state;

	gst_element_class_set_details(gstelement_class, &bpskrcmod_details);

	gobject_class->set_property = gst_bpskrcmod_set_property;
	gobject_class->get_property = gst_bpskrcmod_get_property;

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_bpskrcmod_init(Gst_bpskrcmod *bpskrcmod)
{
	bpskrcmod->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (bpskrcmod->sinkpad, gst_bpskrcmod_chain);
	gst_element_add_pad (GST_ELEMENT(bpskrcmod), bpskrcmod->sinkpad);

	bpskrcmod->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(bpskrcmod), bpskrcmod->srcpad);

	gst_pad_set_setcaps_function(bpskrcmod->sinkpad, gst_bpskrcmod_setcaps);
	gst_pad_use_fixed_caps(bpskrcmod->sinkpad);

	bpskrcmod->lastsymbol = 0.0;
	bpskrcmod->symbollen = -1;
}

GType gst_bpskrcmod_get_type(void)
{
	static GType bpskrcmod_type = 0;

	if (!bpskrcmod_type) {
		static const GTypeInfo bpskrcmod_info = {
			sizeof(Gst_bpskrcmod_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_bpskrcmod_class_init,
			NULL,
			NULL,
			sizeof(Gst_bpskrcmod),
			0,
			(GInstanceInitFunc)gst_bpskrcmod_init,
		};
		bpskrcmod_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstBPSKRCmod", &bpskrcmod_info, 0);
	}
	return bpskrcmod_type;
}
