/*
 *	KISS streamer.
 *
 *	Generates a continuous KISS stream.
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

static GstElementDetails kissstreamer_details = GST_ELEMENT_DETAILS(
	"Quadrature convers",
	"Codec/Decoder/data",
	"Generates a continuous KISS stream",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-tnc-kiss"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-tnc-kiss"
	)
);


static GstFlowReturn gst_kissstreamer_chain(GstPad *pad, GstBuffer *buffer)
{
	Gst_kissstreamer *kstr;
	
printf("chain\n");
	kstr = GST_KISSSTREAMER(GST_OBJECT_PARENT(pad));
	
	while (kstr->inbuf != NULL);
printf("chain done\n");

	kstr->inbuf = buffer;

	return GST_FLOW_OK;
}

static void gst_kissstreamer_loop(GstPad *pad)
{
	Gst_kissstreamer *kstr;
	GstBuffer *buf;
	GstCaps *caps;
	
	caps = gst_pad_get_caps(pad);
	
	kstr = GST_KISSSTREAMER(GST_PAD_PARENT(pad));
	
	while (1/*todo: add de-activate*/) {
		if (kstr->inbuf) {
			printf("buf %d\n", GST_BUFFER_SIZE(kstr->inbuf));
			buf = kstr->inbuf;
			kstr->inbuf = NULL;
		} else {
			printf("fill\n");
			buf = gst_buffer_new_and_alloc(1);
			GST_BUFFER_DATA(buf)[0] = 0xc0;
			gst_buffer_set_caps(buf, caps);
		}
		gst_pad_push(kstr->srcpad, buf);
	}
	
	gst_pad_push(kstr->srcpad, buf);
	
	gst_caps_unref(caps);
}

static gboolean gst_kissstreamer_src_activate_push(GstPad *pad, gboolean active)
{
	Gst_kissstreamer *kstr;
	
	kstr = GST_KISSSTREAMER(GST_OBJECT_PARENT(pad));
	
	if (active) {
		if (gst_pad_is_linked(pad)) {
			return gst_pad_start_task(pad,
			    (GstTaskFunction)gst_kissstreamer_loop, pad);
		} else {
			return FALSE;
		}
	} else {
		return gst_pad_stop_task(pad);
	}
}


static GstElementClass *parent_class = NULL;

static GstStateChangeReturn gst_kissstreamer_change_state(GstElement *element, 
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static void gst_kissstreamer_class_init(Gst_kissstreamer_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	
	gobject_class = (GObjectClass *)klass;
	gstelement_class = (GstElementClass *)klass;
	
	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);
	
	gstelement_class->change_state = gst_kissstreamer_change_state;
	
	gst_element_class_set_details(gstelement_class, &kissstreamer_details);
	
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_kissstreamer_init(Gst_kissstreamer *kissstreamer)
{
	kissstreamer->inbuf = NULL;

	kissstreamer->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&sink_template), "sink");
	
	gst_pad_set_chain_function(kissstreamer->sinkpad, gst_kissstreamer_chain);
	gst_element_add_pad(GST_ELEMENT(kissstreamer), kissstreamer->sinkpad);
	
	kissstreamer->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(kissstreamer), kissstreamer->srcpad);

	gst_pad_set_activatepush_function(kissstreamer->srcpad,
	    gst_kissstreamer_src_activate_push);
}

GType gst_kissstreamer_get_type(void)
{
	static GType kissstreamer_type = 0;

	if (!kissstreamer_type) {
		static const GTypeInfo kissstreamer_info = {
			sizeof(Gst_kissstreamer_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_kissstreamer_class_init,
			NULL,
			NULL,
			sizeof(Gst_kissstreamer),
			0,
			(GInstanceInitFunc)gst_kissstreamer_init,
		};
		kissstreamer_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQKISSstreamer", &kissstreamer_info, 0);
	}
	return kissstreamer_type;
}
