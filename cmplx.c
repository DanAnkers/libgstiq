/*
 *	Quadrature conversion filter.
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

static GstElementDetails iqcmplx_details = GST_ELEMENT_DETAILS(
	"Quadrature conversion",
	"Filter/Effect/Audio",
	"Converts to and from a complex quadrature signal",
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
		"channels = (int) 1;"
		
		"audio/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"width = (int) 32, "
		"rate = (int) [1, MAX ], "
		"channels = (int) 2"
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
		"width = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 1;"
		
		"audio/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"width = (int) 32, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 2"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_iqcmplx_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqcmplx *cmplx;
	GstCaps *caps;

	cmplx = GST_IQCMPLX(gst_pad_get_parent(pad));

	caps = gst_pad_get_caps(cmplx->srcpad);
	gst_buffer_set_caps(buf, caps);
	gst_caps_unref(caps);
	gst_pad_push(cmplx->srcpad, buf);
	gst_object_unref(cmplx);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_iqcmplx_change_state(GstElement *element, 
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqcmplx_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_iqcmplx *cmplx;
	GstCaps *newcaps;
	GstStructure *structure;

	cmplx = GST_IQCMPLX(gst_pad_get_parent(pad));
	newcaps = gst_caps_copy(caps);
	if (!newcaps)
		return FALSE;
	structure = gst_caps_get_structure(newcaps, 0);

	if (!strcmp(gst_structure_get_name(structure), "audio/x-raw-float")) {
		gst_structure_set_name(structure, "audio/x-complex-float");
		gst_structure_set(structure, "channels", G_TYPE_INT, 1, NULL);
		gst_structure_set(structure, "depth", G_TYPE_INT, 64, NULL);
		gst_structure_set(structure, "width", G_TYPE_INT, 64, NULL);
	} else {
		gst_structure_set_name(structure, "audio/x-raw-float");
		gst_structure_set(structure, "channels", G_TYPE_INT, 2, NULL);
		gst_structure_set(structure, "depth", G_TYPE_INT, 32, NULL);
		gst_structure_set(structure, "width", G_TYPE_INT, 32, NULL);
	}
	gst_pad_use_fixed_caps(
	    pad == cmplx->srcpad ? cmplx->sinkpad : cmplx->srcpad);
	return gst_pad_set_caps(
	    pad == cmplx->srcpad ? cmplx->sinkpad : cmplx->srcpad, newcaps);
}

static void gst_iqcmplx_class_init(Gst_iqcmplx_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_iqcmplx_change_state;

	gst_element_class_set_details(gstelement_class, &iqcmplx_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqcmplx_init(Gst_iqcmplx *cmplx)
{
	cmplx->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (cmplx->sinkpad, gst_iqcmplx_chain);
	gst_element_add_pad (GST_ELEMENT(cmplx), cmplx->sinkpad);

	cmplx->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(cmplx), cmplx->srcpad);

	gst_pad_set_setcaps_function(cmplx->srcpad, gst_iqcmplx_setcaps);
	gst_pad_set_setcaps_function(cmplx->sinkpad, gst_iqcmplx_setcaps);
}

GType gst_iqcmplx_get_type(void)
{
	static GType iqcmplx_type = 0;

	if (!iqcmplx_type) {
		static const GTypeInfo iqcmplx_info = {
			sizeof(Gst_iqcmplx_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqcmplx_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqcmplx),
			0,
			(GInstanceInitFunc)gst_iqcmplx_init,
		};
		iqcmplx_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQCmplx", &iqcmplx_info, 0);
	}
	return iqcmplx_type;
}
