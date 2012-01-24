/*
 *	Display FFT as a waterfall.
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

static GstElementDetails waterfall_details = GST_ELEMENT_DETAILS(
	"Waterfall plugin",
	"Filter/Effect/Video",
	"Convert FFT to Waterfall",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-fft-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"length = (int) [ 1, MAX ], "
		"channels = (int) 1"
	)
);

enum {
	ARG_0,
	ARG_MARKER
};

static GstPadTemplate *src_template;

static GstElementClass *parent_class = NULL;

static void gst_waterfall_base_init(Gst_waterfall_class *klass)
{
	GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
	GstCaps *capslist;
	gulong format = GST_MAKE_FOURCC('I', '4', '2', '0');

	capslist = gst_caps_new_empty();
	gst_caps_append_structure(capslist,
	    gst_structure_new("video/x-raw-yuv",
	        "format", GST_TYPE_FOURCC, format,
		"width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
		"height", G_TYPE_INT, 128,
		"framerate", GST_TYPE_FRACTION_RANGE, 1, 1, 50, 1, NULL));

	src_template = gst_pad_template_new("src", GST_PAD_SRC,
	     GST_PAD_ALWAYS, capslist);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class, src_template);

	gst_element_class_set_details(gstelement_class, &waterfall_details);
}

static GstFlowReturn gst_waterfall_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_waterfall *waterfall;
	GstBuffer *outbuf;
	GstCaps *caps;
	int i;

	waterfall = GST_WATERFALL(gst_pad_get_parent(pad));

	if (++waterfall->fcnt >= waterfall->factor && waterfall->buffer) {
		unsigned char pix;
		waterfall->fcnt = 0;
		memmove(waterfall->buffer, waterfall->buffer+waterfall->length,
		    waterfall->uoff - waterfall->length);
		if (waterfall->frame & 1) {
			memmove(waterfall->buffer + waterfall->uoff,
		  	  waterfall->buffer + waterfall->uoff + waterfall->length / 2,
			    (waterfall->uoff/2 - waterfall->length)/ 2);
			memmove(waterfall->buffer + waterfall->voff,
			    waterfall->buffer + waterfall->voff + waterfall->length / 2,
			    (waterfall->uoff/2 - waterfall->length)/ 2);
		}
		waterfall->frame++;
		for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(float)/2; i++) {
			pix =
			    hypot(((float*)GST_BUFFER_DATA(buf))[i*2],
			    ((float*)GST_BUFFER_DATA(buf))[i*2+1])
			    * waterfall->length;
			
			pix = 46 * log(pix);
			//pix = 255 - (255 - pix) * (255 - pix) / 255;
			if (i < GST_BUFFER_SIZE(buf)/sizeof(float)/4) {
				waterfall->buffer[
				    waterfall->uoff-waterfall->length/2+i]=
				    pix;
			} else {
				waterfall->buffer[
				    waterfall->uoff-(waterfall->length*3)/2+i]=
				    pix;
			}
		}
		if (waterfall->frame&1) for (i=0; i<waterfall->length/2; i++) {
			pix = waterfall->buffer[waterfall->uoff-1-i*2]/2;
			waterfall->buffer[waterfall->voff-1-i]=
			    128 + (63 - pix) * pix / 16;
			waterfall->buffer[waterfall->size-1-i]=
			    128 + pix * pix / 128;
		}
		waterfall->buffer[waterfall->voff-waterfall->length/4] = 64;
		waterfall->buffer[waterfall->size-waterfall->length/4] = 64;
		if (waterfall->marker) {
			waterfall->buffer[waterfall->voff-waterfall->length/4 +
			    waterfall->marker] = 0;
			waterfall->buffer[waterfall->size-waterfall->length/4 +
			    waterfall->marker] = 0;
		}
		outbuf = gst_buffer_new_and_alloc(waterfall->size);
		memcpy(GST_BUFFER_DATA(outbuf), waterfall->buffer,
		    waterfall->size);
		caps = gst_pad_get_caps(waterfall->srcpad);
		gst_buffer_set_caps(outbuf, caps);
		gst_caps_unref(caps);
		GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
		GST_BUFFER_OFFSET(outbuf) = waterfall->offset++;
		gst_pad_push(waterfall->srcpad, outbuf);
	}
	gst_buffer_unref(buf);
	gst_object_unref(waterfall);
	return GST_FLOW_OK;
}

static int gst_waterfall_setup(Gst_waterfall *waterfall)
{
	int i;

	if (waterfall->buffer) {
		free(waterfall->buffer);
		waterfall->buffer = NULL;
	}
	waterfall->size = (waterfall->length * waterfall->height * 3) / 2;
	waterfall->buffer = malloc(waterfall->size);
	if (waterfall->buffer == NULL)
		return -1;
	waterfall->uoff = waterfall->length * waterfall->height;
	memset(waterfall->buffer, 0, waterfall->uoff);
	waterfall->voff = (waterfall->uoff * 5) / 4;
	memset(waterfall->buffer + waterfall->uoff, 128, waterfall->uoff / 2);
	for (i = 0; i < waterfall->height; i++) {
		waterfall->buffer[waterfall->length*i +waterfall->length/2] =0;
		waterfall->buffer[waterfall->length/2*i/2 + waterfall->uoff]=64;
		waterfall->buffer[waterfall->length/2*i/2 + waterfall->voff]=64;
	}
	waterfall->marker = 0.5 +
	    waterfall->markerf / (float)waterfall->rate *
	    waterfall->length / 2;
	waterfall->marker %= waterfall->length;
	return 0;
}

static void gst_waterfall_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_waterfall *waterfall;

	g_return_if_fail(GST_IS_WATERFALL(object));
	waterfall = GST_WATERFALL(object);

	switch(prop_id) {
		case ARG_MARKER:
			waterfall->markerf = g_value_get_float(value);
			waterfall->marker = 0.5 +
			    waterfall->markerf / (float)waterfall->rate *
			    waterfall->length / 2;
			waterfall->marker %= waterfall->length;
			break;
		default:
			break;
	}
}

static void gst_waterfall_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_waterfall *waterfall;

	g_return_if_fail(GST_IS_WATERFALL(object));
	waterfall = GST_WATERFALL(object);

	switch(prop_id) {
		case ARG_MARKER:
			g_value_set_float(value, waterfall->markerf);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static GstStateChangeReturn gst_waterfall_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_waterfall_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_waterfall *waterfall;
	GstStructure *structure;
	GstCaps *newcaps;
	gboolean ret;

	waterfall = GST_WATERFALL(gst_pad_get_parent(pad));

	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &waterfall->rate);
	gst_structure_get_int(structure, "length", &waterfall->length);

	if (waterfall->length & 3)
		return FALSE;

	waterfall->factor = 1;
	if (waterfall->rate / waterfall->length >= 10) {
		while ((float)waterfall->rate/(float)waterfall->length/
		    (float)waterfall->factor > 50.0)
			waterfall->factor++;
	}
	waterfall->fcnt = 0;

	gst_waterfall_setup(waterfall);

	newcaps = gst_caps_copy(
	    gst_pad_get_pad_template_caps(waterfall->srcpad));
	structure = gst_caps_get_structure(newcaps, 0);
	gst_structure_set(structure, "framerate", GST_TYPE_FRACTION, 
	    waterfall->rate, waterfall->length * waterfall->factor, NULL);
	gst_structure_set(structure, "width", G_TYPE_INT, waterfall->length,
	    NULL);
	gst_pad_use_fixed_caps(waterfall->srcpad);
	ret = gst_pad_set_caps(waterfall->srcpad, newcaps);
	gst_object_unref(waterfall);

	return ret;
//	return gst_pad_try_set_caps(
//	    (pad == waterfall->srcpad) ? waterfall->sinkpad : waterfall->srcpad,
//	    gst_caps_copy(caps));
}

static void gst_waterfall_class_init(Gst_waterfall_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_waterfall_set_property;
	gobject_class->get_property = gst_waterfall_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_MARKER,
	    g_param_spec_float("marker", "marker", "marker", -G_MAXFLOAT, G_MAXFLOAT, 0.0, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_waterfall_change_state;
}

static void gst_waterfall_init(Gst_waterfall *waterfall)
{
	waterfall->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&sink_template), "sink");

	gst_pad_set_chain_function (waterfall->sinkpad, gst_waterfall_chain);
	gst_element_add_pad(GST_ELEMENT(waterfall), waterfall->sinkpad);

	waterfall->srcpad = gst_pad_new_from_template(src_template, "src");
	gst_element_add_pad(GST_ELEMENT(waterfall), waterfall->srcpad);

	gst_pad_set_setcaps_function(waterfall->sinkpad, gst_waterfall_setcaps);
//	gst_pad_set_setcaps_function(waterfall->srcpad, gst_waterfall_setcaps);

	waterfall->length = 1024;
	waterfall->height = 128;
	waterfall->frame = 0;
	waterfall->buffer = NULL;
	waterfall->offset = 0;
}

GType gst_waterfall_get_type(void)
{
	static GType waterfall_type = 0;

	if (!waterfall_type) {
		static const GTypeInfo waterfall_info = {
			sizeof(Gst_waterfall_class),
			(GBaseInitFunc) gst_waterfall_base_init,
			NULL,
			(GClassInitFunc)gst_waterfall_class_init,
			NULL,
			NULL,
			sizeof(Gst_waterfall),
			0,
			(GInstanceInitFunc)gst_waterfall_init,
		};
		waterfall_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstWaterfall", &waterfall_info, 0);
	}
	return waterfall_type;
}
