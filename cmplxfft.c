/*
 *	Complex FFT.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#include <math.h>
#include <fftw3.h>
#include "gstiq.h"
#include <string.h>

static GstElementDetails cmplxfft_details = GST_ELEMENT_DETAILS(
	"Complex FFT plugin",
	"Filter/Effect/Audio",
	"Complex FFT",
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
		"channels = (int) 1, "
		"rate = (int) [ 1, MAX ] "
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-fft-float, "
		"depth = (int) 64, "
		"rate = (int) [ 1, MAX ], "
		"channels = (int) 1, "
		"length = (int) [ 1, MAX ], "
		"endianness = (int) BYTE_ORDER "
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_cmplxfft_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_cmplxfft *cmplxfft;
	GstBuffer *outbuf;
	GstCaps *caps;
	int i, j;

	cmplxfft = GST_CMPLXFFT(gst_pad_get_parent(pad));
	if (cmplxfft->buffer) {
		j = 0;
		while (j < GST_BUFFER_SIZE(buf)/sizeof(fftwf_complex)) {
			i = GST_BUFFER_SIZE(buf)/sizeof(fftwf_complex) - j;
			if (i > cmplxfft->length - cmplxfft->fill)
				i = cmplxfft->length - cmplxfft->fill;
			memcpy(cmplxfft->buffer + cmplxfft->fill,
			    GST_BUFFER_DATA(buf) + j * sizeof(fftwf_complex),
			    i * sizeof(fftwf_complex));
			j += i;
			cmplxfft->fill += i;
			if (cmplxfft->fill >= cmplxfft->length) {
				int k;
				cmplxfft->fill = 0;
				outbuf = gst_buffer_new_and_alloc(
				    cmplxfft->length * sizeof(fftwf_complex));
				fftwf_execute(cmplxfft->plan);
				memcpy(GST_BUFFER_DATA(outbuf), cmplxfft->buffer,
				    sizeof(fftwf_complex) * cmplxfft->length);
				for (k = 0; k < cmplxfft->length * 2; k++) {
					((float *)GST_BUFFER_DATA(outbuf))[k]
					    /= cmplxfft->length;
				}
				caps = gst_pad_get_caps(cmplxfft->srcpad);
				gst_buffer_set_caps(outbuf, caps);
				gst_caps_unref(caps);
				GST_BUFFER_OFFSET(outbuf) = cmplxfft->offset;
				cmplxfft->offset++;
				GST_BUFFER_TIMESTAMP(outbuf) =
				    GST_BUFFER_TIMESTAMP(buf);
				gst_pad_push(cmplxfft->srcpad, outbuf);
			}
		}
	}
	gst_buffer_unref(buf);
	gst_object_unref(cmplxfft);
	return GST_FLOW_OK;
}

static int gst_cmplxfft_setup(Gst_cmplxfft *cmplxfft)
{
	int n = cmplxfft->length;

	if (cmplxfft->buffer) {
		fftwf_free(cmplxfft->buffer);
		fftwf_destroy_plan(cmplxfft->plan);
	}
	cmplxfft->buffer = NULL;
	if (cmplxfft->length < 2)
		return -1;
	cmplxfft->buffer = fftwf_malloc(sizeof(fftwf_complex) * n);
	if (!cmplxfft->buffer)
		return -1;
	cmplxfft->plan = fftwf_plan_dft_1d(n, cmplxfft->buffer,
	    cmplxfft->buffer, FFTW_FORWARD, FFTW_MEASURE);
	cmplxfft->fill = 0;
	return 0;
}

static GstStateChangeReturn gst_cmplxfft_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_cmplxfft_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_cmplxfft *cmplxfft;
	GstCaps *newcaps;
	GstStructure *structure;
	gint rate;
	gboolean ret;

	cmplxfft = GST_CMPLXFFT(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &rate);

	if (pad == cmplxfft->srcpad) {
		gst_structure_get_int(structure, "length", &cmplxfft->length);
		gst_cmplxfft_setup(cmplxfft);
		newcaps = gst_caps_copy(gst_pad_get_caps(cmplxfft->sinkpad));
	} else {
		newcaps = gst_pad_get_allowed_caps(cmplxfft->srcpad);
	}
	structure = gst_caps_get_structure(newcaps, 0);
	gst_structure_set(structure, "rate", G_TYPE_INT, rate, NULL);

	gst_pad_use_fixed_caps(pad == cmplxfft->sinkpad ? 
	    cmplxfft->srcpad : cmplxfft->sinkpad);
	ret = gst_pad_set_caps(pad == cmplxfft->sinkpad ? 
	    cmplxfft->srcpad : cmplxfft->sinkpad, newcaps);
	gst_object_unref(cmplxfft);
	return ret;
}

static void gst_cmplxfft_class_init(Gst_cmplxfft_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_cmplxfft_change_state;

	gst_element_class_set_details(gstelement_class, &cmplxfft_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_cmplxfft_init(Gst_cmplxfft *cmplxfft)
{
	cmplxfft->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (cmplxfft->sinkpad, gst_cmplxfft_chain);
	gst_element_add_pad(GST_ELEMENT(cmplxfft), cmplxfft->sinkpad);

	cmplxfft->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(cmplxfft), cmplxfft->srcpad);

	gst_pad_set_setcaps_function(cmplxfft->sinkpad, gst_cmplxfft_setcaps);
	gst_pad_set_setcaps_function(cmplxfft->srcpad, gst_cmplxfft_setcaps);

	cmplxfft->buffer = NULL;
	cmplxfft->length = 512;
	gst_cmplxfft_setup(cmplxfft);
	cmplxfft->offset = 0;
}

GType gst_cmplxfft_get_type(void)
{
	static GType cmplxfft_type = 0;

	if (!cmplxfft_type) {
		static const GTypeInfo cmplxfft_info = {
			sizeof(Gst_cmplxfft_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_cmplxfft_class_init,
			NULL,
			NULL,
			sizeof(Gst_cmplxfft),
			0,
			(GInstanceInitFunc)gst_cmplxfft_init,
		};
		cmplxfft_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstComplexFFT", &cmplxfft_info, 0);
	}
	return cmplxfft_type;
}
