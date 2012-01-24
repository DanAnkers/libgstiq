/*
 *	Display a complex signal on an XY-scope.
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

static GstElementDetails vector_details = GST_ELEMENT_DETAILS(
	"Vectorscope plugin",
	"Filter/Effect/Video",
	"Convert a complex signal to an XY image",
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
		"channels = (int) 1; "

		"audio/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"width = (int) 32, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 2"
	)
);

enum {
	ARG_0,
	ARG_DECAY
};

static GstPadTemplate *src_template;

static GstElementClass *parent_class = NULL;

static void gst_vectorscope_base_init(Gst_vectorscope_class *klass)
{
	GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
	GstCaps *capslist;
	gulong format = GST_MAKE_FOURCC('I', '4', '2', '0');

	capslist = gst_caps_new_empty();
	gst_caps_append_structure(capslist,
	    gst_structure_new("video/x-raw-yuv",
	        "format", GST_TYPE_FOURCC, format,
		"width", G_TYPE_INT, 256,
		"height", G_TYPE_INT, 256,
		"framerate", GST_TYPE_FRACTION, 25, 1, NULL));

	src_template = gst_pad_template_new("src", GST_PAD_SRC,
	     GST_PAD_ALWAYS, capslist);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class, src_template);

	gst_element_class_set_details(gstelement_class, &vector_details);
}

static GstFlowReturn gst_vectorscope_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_vectorscope *vector;
	GstBuffer *outbuf;
	GstCaps *caps;
	float *val;
	float decay;
	int xcent, ycent, x, y;
	int i;

	vector = GST_VECTORSCOPE(gst_pad_get_parent(pad));
	decay = vector->decay;

	val = (float *)GST_BUFFER_DATA(buf);
	xcent = vector->width/2;
	ycent = vector->height/2;
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(float); i+=2) {
		x = val[i] * xcent + xcent;
		y = ycent - val[i+1] * ycent;
		if (x < 0)
			x = 0;
		if (x >= vector->width)
			x = vector->width - 1;
		if (y < 0)
			y = 0;
		if (y >= vector->height)
			y = vector->height - 1;
		vector->buffer[y * vector->width + x] =255;
		vector->samples++;
		if (vector->samples == vector->framesamples) {
			outbuf = gst_buffer_new_and_alloc(vector->size);
			memcpy(GST_BUFFER_DATA(outbuf), vector->buffer,
			    vector->size);
			caps = gst_pad_get_caps(vector->srcpad);
			gst_buffer_set_caps(outbuf, caps);
			gst_caps_unref(caps);
			GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
			GST_BUFFER_OFFSET(outbuf) = vector->offset++;
			gst_pad_push(vector->srcpad, outbuf);
			vector->samples = 0;
			for (i = 0; i < vector->uoff; i++)
				vector->buffer[i] *= decay;
		}
	}

	gst_buffer_unref(buf);
	gst_object_unref(vector);
	return GST_FLOW_OK;
}

static int gst_vectorscope_setup(Gst_vectorscope *vector)
{
	int i;
	int csq;
	int x, y;
	int xsq, xysq;
	int vlen;

	if (vector->buffer) {
		free(vector->buffer);
		vector->buffer = NULL;
	}
	vector->size = (vector->width * vector->height * 3) / 2;
	vector->buffer = malloc(vector->size);
	if (vector->buffer == NULL)
		return -1;
	memset(vector->buffer, 0, vector->size);
	vector->uoff = vector->width * vector->height;
	vector->voff = vector->uoff + vector->width * vector->height / 4;
	memset(vector->buffer + vector->uoff, 140,
	    vector->width * vector->height / 4);
	memset(vector->buffer + vector->voff, 128,
	    vector->width * vector->height / 4);
	for (i = 0; i < vector->width / 2; i++) {
		vector->buffer[vector->uoff + 
		    vector->width / 2 * vector->height / 4 + i] = 0;
		vector->buffer[vector->voff + 
		    vector->width / 2 * vector->height / 4 + i] = 0;
	}
	for (i = 0; i < vector->height / 2; i++) {
		vector->buffer[vector->uoff + 
		    vector->width / 4 + i * vector->width / 2] = 0;
		vector->buffer[vector->voff + 
		    vector->width / 4 + i * vector->width / 2] = 0;
	}

	csq = vector->width / 4;
	csq *= csq;
	y = vector->height / 4;
	vlen = vector->width / 2;
	for (x = 0; x < y; ) {
		xsq = (x + 1)*(x + 1) + y*y;
		xysq = (x + 1)*(x + 1) + (y - 1)*(y - 1);
		if (abs(xsq - csq) <= abs(xysq - csq)) {
			x++;
		} else {
			x++;
			y--;
		}
		if (y < 0)
			y = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 +
		     x - y * vlen - 1] = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 +
		     y - x * vlen - 1] = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 -
		     x - y * vlen] = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 -
		     y - x * vlen] = 0;

		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 +
		     x + (y - 1) * vlen - 1] = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 +
		     y + (x - 1) * vlen - 1] = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 -
		     x + (y - 1) * vlen] = 0;
		vector->buffer[
		     vector->voff+vlen*vector->height/4 +
		     vector->height/4 -
		     y + (x - 1) * vlen] = 0;

		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 +
		     x - y * vlen - 1] = 0;
		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 +
		     y - x * vlen - 1] = 0;
		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 -
		     x - y * vlen] = 0;
		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 -
		     y - x * vlen] = 0;

		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 +
		     x + (y - 1) * vlen - 1] = 0;
		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 +
		     y + (x - 1) * vlen - 1] = 0;
		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 -
		     x + (y - 1) * vlen] = 0;
		vector->buffer[
		     vector->uoff+vlen*vector->height/4 +
		     vector->height/4 -
		     y + (x - 1) * vlen] = 0;
	}
	return 0;
}

static void gst_vectorscope_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_vectorscope *vector;

	g_return_if_fail(GST_IS_VECTORSCOPE(object));
	vector = GST_VECTORSCOPE(object);

	switch(prop_id) {
		case ARG_DECAY:
			vector->decay = g_value_get_float(value);
			break;
		default:
			break;
	}
}

static void gst_vectorscope_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_vectorscope *vector;

	g_return_if_fail(GST_IS_VECTORSCOPE(object));
	vector = GST_VECTORSCOPE(object);

	switch(prop_id) {
		case ARG_DECAY:
			g_value_set_float(value, vector->decay);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_vectorscope_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_vectorscope_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_vectorscope *vector;
	GstStructure *structure;
	GstCaps *newcaps;
	gboolean ret;

	vector = GST_VECTORSCOPE(gst_pad_get_parent(pad));

	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &vector->rate);
	vector->framesamples = vector->rate/25;

	gst_vectorscope_setup(vector);

	newcaps = gst_caps_copy(
	    gst_pad_get_pad_template_caps(vector->srcpad));
//	structure = gst_caps_get_structure(newcaps, 0);
//	gst_structure_set(structure, "framerate", G_TYPE_DOUBLE, 
//	    (double)vector->rate / (double)vector->width, NULL);
	gst_pad_use_fixed_caps(vector->srcpad);
	ret = gst_pad_set_caps(vector->srcpad, newcaps);
	gst_object_unref(vector);
	return ret;
}

static void gst_vectorscope_class_init(Gst_vectorscope_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_vectorscope_set_property;
	gobject_class->get_property = gst_vectorscope_get_property;

	g_object_class_install_property(G_OBJECT_CLASS(klass), ARG_DECAY,
	    g_param_spec_float("decay", "decay", "decay", 0.0, 1.0, 0.9,
	    G_PARAM_READWRITE));

	gstelement_class->change_state = gst_vectorscope_change_state;
}

static void gst_vectorscope_init(Gst_vectorscope *vector)
{
	vector->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&sink_template), "sink");

	gst_pad_set_chain_function (vector->sinkpad, gst_vectorscope_chain);
	gst_element_add_pad(GST_ELEMENT(vector), vector->sinkpad);

	vector->srcpad = gst_pad_new_from_template(src_template, "src");
	gst_element_add_pad(GST_ELEMENT(vector), vector->srcpad);

	gst_pad_set_setcaps_function(vector->sinkpad, gst_vectorscope_setcaps);

	vector->width = 256;
	vector->height = 256;
	vector->samples = 0;
	vector->buffer = NULL;
	vector->offset = 0;
	vector->decay = 0.9;
}

GType gst_vectorscope_get_type(void)
{
	static GType vector_type = 0;

	if (!vector_type) {
		static const GTypeInfo vector_info = {
			sizeof(Gst_vectorscope_class),
			(GBaseInitFunc)gst_vectorscope_base_init,
			NULL,
			(GClassInitFunc)gst_vectorscope_class_init,
			NULL,
			NULL,
			sizeof(Gst_vectorscope),
			0,
			(GInstanceInitFunc)gst_vectorscope_init,
		};
		vector_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstVectorScope", &vector_info, 0);
	}
	return vector_type;
}
