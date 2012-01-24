/*
 *	Manchester modulator.
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

static GstElementDetails manchestermod_details = GST_ELEMENT_DETAILS(
	"Manchester modulator plugin",
	"Filter/Effect/Audio",
	"Manchester modulator",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"width = (int) 32, "
		"rate = (int) [ 1, MAX ], "
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
		"buffer-frames = (int) 0, "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_manchestermod_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_manchestermod *mod;
	GstBuffer *outbuf;
	GstCaps *caps;
	gfloat *bufout, *bufin;
	int i, j;

	mod = GST_MANCHESTERMOD(gst_pad_get_parent(pad));

	caps = gst_pad_get_caps(mod->srcpad);
	if (!gst_caps_is_fixed(caps) || mod->symbollen < 0) {
		GstStructure *structure;
		gst_caps_unref(caps);
		caps = gst_pad_get_allowed_caps(mod->srcpad);
		structure = gst_caps_get_structure(caps, 0);
		gst_structure_get_int(structure, "rate", &mod->rate);
		mod->symbollen =
		    (float)mod->rate/(float)mod->symbolrate;
		gst_pad_use_fixed_caps(mod->srcpad);
	}

	outbuf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf) * mod->symbollen);
	GST_BUFFER_OFFSET(outbuf) = GST_BUFFER_OFFSET(buf);
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);

	bufin = (gfloat *)GST_BUFFER_DATA(buf);
	bufout = (gfloat *)GST_BUFFER_DATA(outbuf);
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(gfloat); i ++) {
		for (j = 0; j < mod->symbollen; j++) {
			bufout[(i * mod->symbollen + j)] =
				sin((j * M_PI * 2) / mod->symbollen) *
				bufin[i] * 0.999;
		}
	}

	gst_buffer_unref(buf);
	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);
	gst_pad_push(mod->srcpad, outbuf);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_manchestermod_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static void gst_manchester_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
}

static void gst_manchester_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	switch(prop_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static gboolean gst_manchestermod_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_manchestermod *mod;

	mod = GST_MANCHESTERMOD(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &mod->symbolrate);

	return TRUE;
}

static void gst_manchestermod_class_init(Gst_manchestermod_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_manchestermod_change_state;

	gst_element_class_set_details(gstelement_class, &manchestermod_details);

	gobject_class->set_property = gst_manchester_set_property;
	gobject_class->get_property = gst_manchester_get_property;

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_manchestermod_init(Gst_manchestermod *mod)
{
	mod->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (mod->sinkpad, gst_manchestermod_chain);
	gst_element_add_pad (GST_ELEMENT(mod), mod->sinkpad);

	mod->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(mod), mod->srcpad);

	gst_pad_set_setcaps_function(mod->sinkpad, gst_manchestermod_setcaps);
	gst_pad_use_fixed_caps(mod->sinkpad);

	mod->symbollen = -1;
}

GType gst_manchestermod_get_type(void)
{
	static GType manchestermod_type = 0;

	if (!manchestermod_type) {
		static const GTypeInfo manchestermod_info = {
			sizeof(Gst_manchestermod_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_manchestermod_class_init,
			NULL,
			NULL,
			sizeof(Gst_manchestermod),
			0,
			(GInstanceInitFunc)gst_manchestermod_init,
		};
		manchestermod_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstManchestermod", &manchestermod_info, 0);
	}
	return manchestermod_type;
}
