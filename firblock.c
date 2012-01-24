/*
 *	Simple rectangular block FIR filter.
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

static GstElementDetails firblock_details = GST_ELEMENT_DETAILS(
	"Rectangular FIR filter plugin",
	"Filter/Effect/Audio",
	"Rectangular block FIR filter",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0,
	ARG_FREQUENCY,
	ARG_DEPTH
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) [ 1, MAX ];"
		
		"audio/x-complex-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) [ 1, MAX ]"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-raw-float, "
		"depth = (int) 32, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) [ 1, MAX ],"
		"endianness = (int) BYTE_ORDER; "
		
		"audio/x-complex-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"rate = (int) [ 1, MAX], "
		"channels = (int) [ 1, MAX ]"
	)
);

static GstElementClass *parent_class = NULL;

gfloat firblockfilterpass(struct firblockfilter *filter, gfloat in)
{
	in /= (float)filter->size;
	filter->sum += in;
	filter->sum -= filter->buffer[filter->index];
	filter->buffer[filter->index] = in;
	filter->index++;
	if (filter->index >= filter->size)
		filter->index = 0;
	return filter->sum;
}

struct firblockfilter *firblockfilteralloc(int nr, int size)
{
	struct firblockfilter *filters;
	int i;

	filters = malloc(sizeof(struct firblockfilter) * nr);
	if (!filters)
		return NULL;
	for (i = 0; i < nr; i++) {
		filters[i].sum = 0.0;
		filters[i].buffer = malloc(sizeof(gfloat) * size);
		if (!filters[i].buffer)
			goto err;
		memset(filters[i].buffer, 0, sizeof(gfloat) * size);
		filters[i].size = size;
		filters[i].index = 0;
	}
	return filters;
err:
	for (i--; i >= 0; i--)
		free(filters[i].buffer);
	free(filters);
	return NULL;
}

void firblockfilterfree(struct firblockfilter *filters, int nr)
{
	int i;

	for (i = 0; i < nr; i++)
		free(filters[i].buffer);
	free(filters);
}

void firblockfilterrealloc(Gst_firblock *firblock)
{
	float size;
	if (firblock->filters)
		firblockfilterfree(firblock->filters, firblock->nr);
	firblock->filters = NULL;

	if (!firblock->frequency)
		return;
	if (!firblock->rate)
		return;
	if (!firblock->channels)
		return;
	size = (float)firblock->rate / (float)firblock->frequency;
	size /= 2;
	size += 0.5;
	firblock->size = size;
	firblock->nr = firblock->channels * firblock->depth;
	if (firblock->size == 0)
		return;
	firblock->filters = firblockfilteralloc(firblock->nr, firblock->size);
}

static GstFlowReturn gst_firblock_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_firblock *firblock;
	GstBuffer *outbuf;
	GstCaps *caps;
	int channels, depth;
	gfloat *val, *out;
	int i, j, k;

	firblock = GST_FIRBLOCK(gst_pad_get_parent(pad));

	caps = gst_pad_get_caps(firblock->srcpad);

	if (firblock->filters != 0) {
		if (!gst_buffer_is_writable(buf)) {
			outbuf = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(buf));
			GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
			GST_BUFFER_OFFSET(outbuf) = GST_BUFFER_OFFSET(buf);
		} else {
			outbuf = buf;
		}
		channels = firblock->channels;
		depth = firblock->depth;
		val = (gfloat *)GST_BUFFER_DATA(buf);
		out = (gfloat *)GST_BUFFER_DATA(outbuf);
		for (i = 0; i < GST_BUFFER_SIZE(outbuf)/sizeof(gfloat);
		    i += channels) {
			for (j = 0; j < channels; j++) {
				out[i+j] = val[i+j];
				for (k = 0; k < depth; k++) {
					out[i+j] = firblockfilterpass(
					    &firblock->filters[j*depth + k],
					    out[i+j]);
				}
			}
		}
		if (buf != outbuf)
			gst_buffer_unref(buf);
		gst_buffer_set_caps(outbuf, caps);
		gst_pad_push(firblock->srcpad, outbuf);
	} else {
		gst_buffer_set_caps(buf, caps);
		gst_pad_push(firblock->srcpad, buf);
	}
	gst_caps_unref(caps);
	gst_object_unref(firblock);
	return GST_FLOW_OK;
}

static void gst_firblock_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_firblock *firblock;

	g_return_if_fail(GST_IS_FIRBLOCK(object));
	firblock = GST_FIRBLOCK(object);

	switch(prop_id) {
		case ARG_FREQUENCY:
			firblock->frequency = g_value_get_int(value);
			firblockfilterrealloc(firblock);
			break;
		case ARG_DEPTH:
			firblock->depth = g_value_get_int(value);
			firblockfilterrealloc(firblock);
			break;
		default:
			break;
	}
}

static void gst_firblock_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_firblock *firblock;

	g_return_if_fail(GST_IS_FIRBLOCK(object));
	firblock = GST_FIRBLOCK(object);

	switch(prop_id) {
		case ARG_FREQUENCY:
			g_value_set_int(value, firblock->frequency);
			break;
		case ARG_DEPTH:
			g_value_set_int(value, firblock->depth);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_firblock_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_firblock_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_firblock *firblock;
	gboolean ret;

	firblock = GST_FIRBLOCK(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &firblock->rate);
	gst_structure_get_int(structure, "channels", &firblock->channels);
	if (!strcmp(gst_structure_get_name(structure), "audio/x-complex-float"))
		firblock->channels *= 2;

	firblockfilterrealloc(firblock);
	gst_pad_use_fixed_caps(pad == firblock->srcpad ? firblock->sinkpad :
	    firblock->srcpad);
	ret = gst_pad_set_caps(
	    (pad == firblock->srcpad) ? firblock->sinkpad : firblock->srcpad,
	    gst_caps_copy(caps));
	gst_object_unref(firblock);
	return ret;
}

static void gst_firblock_class_init(Gst_firblock_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_firblock_set_property;
	gobject_class->get_property = gst_firblock_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_FREQUENCY,
	    g_param_spec_int("frequency", "frequency", "frequency",
	         -G_MAXINT, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_DEPTH,
	    g_param_spec_int("depth", "depth", "depth",
	         1, G_MAXINT, 1, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_firblock_change_state;

	gst_element_class_set_details(gstelement_class, &firblock_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_firblock_init(Gst_firblock *firblock)
{
	firblock->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (firblock->sinkpad, gst_firblock_chain);
	gst_element_add_pad (GST_ELEMENT(firblock), firblock->sinkpad);

	firblock->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(firblock), firblock->srcpad);

	gst_pad_set_setcaps_function(firblock->sinkpad, gst_firblock_setcaps);
	gst_pad_set_setcaps_function(firblock->srcpad, gst_firblock_setcaps);

	firblock->frequency = 0;
	firblock->size = 0;
	firblock->depth = 1;
	firblock->size = 0;
	firblock->filters = NULL;
}

GType gst_firblock_get_type(void)
{
	static GType firblock_type = 0;

	if (!firblock_type) {
		static const GTypeInfo firblock_info = {
			sizeof(Gst_firblock_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_firblock_class_init,
			NULL,
			NULL,
			sizeof(Gst_firblock),
			0,
			(GInstanceInitFunc)gst_firblock_init,
		};
		firblock_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstFIRblock", &firblock_info, 0);
	}
	return firblock_type;
}
