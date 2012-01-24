/*
 *	Complex Reverse FFT.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2006
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

static GstElementDetails cmplxrfft_details = GST_ELEMENT_DETAILS(
	"Complex Reverse FFT plugin",
	"Filter/Effect/Audio",
	"Complex Reverse FFT",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-complex-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"channels = (int) 1, "
		"rate = (int) [ 1, MAX ] "
	)
);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
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

static GstFlowReturn gst_cmplxrfft_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_cmplxrfft *cmplxrfft;
	GstBuffer *outbuf;
	GstCaps *caps;
	int i, j;

	cmplxrfft = GST_CMPLXRFFT(gst_pad_get_parent(pad));
	if (cmplxrfft->buffer) {
		j = 0;
		while (j < GST_BUFFER_SIZE(buf)/sizeof(fftwf_complex)) {
			i = GST_BUFFER_SIZE(buf)/sizeof(fftwf_complex) - j;
			if (i > cmplxrfft->length - cmplxrfft->fill)
				i = cmplxrfft->length - cmplxrfft->fill;
			memcpy(cmplxrfft->buffer + cmplxrfft->fill,
			    GST_BUFFER_DATA(buf) + j * sizeof(fftwf_complex),
			    i * sizeof(fftwf_complex));
			j += i;
			cmplxrfft->fill += i;
			if (cmplxrfft->fill >= cmplxrfft->length) {
				cmplxrfft->fill = 0;
				outbuf = gst_buffer_new_and_alloc(
				    cmplxrfft->length * sizeof(fftwf_complex));
				fftwf_execute(cmplxrfft->plan);
				memcpy(GST_BUFFER_DATA(outbuf), cmplxrfft->buffer,
				    sizeof(fftwf_complex) * cmplxrfft->length);
				caps = gst_pad_get_caps(cmplxrfft->srcpad);
				gst_buffer_set_caps(outbuf, caps);
				gst_caps_unref(caps);
				GST_BUFFER_OFFSET(outbuf) = cmplxrfft->offset;
				cmplxrfft->offset++;
				GST_BUFFER_TIMESTAMP(outbuf) =
				    GST_BUFFER_TIMESTAMP(buf);
				gst_pad_push(cmplxrfft->srcpad, outbuf);
			}
		}
	}
	gst_buffer_unref(buf);
	gst_object_unref(cmplxrfft);
	return GST_FLOW_OK;
}

static int gst_cmplxrfft_setup(Gst_cmplxrfft *cmplxrfft)
{
	int n = cmplxrfft->length;

	if (cmplxrfft->buffer) {
		fftwf_free(cmplxrfft->buffer);
		fftwf_destroy_plan(cmplxrfft->plan);
	}
	cmplxrfft->buffer = NULL;
	if (cmplxrfft->length < 2)
		return -1;
	cmplxrfft->buffer = fftwf_malloc(sizeof(fftwf_complex) * n);
	if (!cmplxrfft->buffer)
		return -1;
	cmplxrfft->plan = fftwf_plan_dft_1d(n, cmplxrfft->buffer,
	    cmplxrfft->buffer, FFTW_BACKWARD, FFTW_MEASURE);
	cmplxrfft->fill = 0;
	return 0;
}

static GstStateChangeReturn gst_cmplxrfft_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_cmplxrfft_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_cmplxrfft *cmplxrfft;
	GstCaps *newcaps;
	GstStructure *structure;
	gint rate;
	gboolean ret;

	cmplxrfft = GST_CMPLXRFFT(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &rate);
	gst_structure_get_int(structure, "length", &cmplxrfft->length);

	gst_cmplxrfft_setup(cmplxrfft);

	newcaps = gst_caps_copy(gst_pad_get_caps(cmplxrfft->srcpad));
	structure = gst_caps_get_structure(newcaps, 0);
	gst_structure_set(structure, "rate", G_TYPE_INT, rate, NULL);

	gst_pad_use_fixed_caps(cmplxrfft->srcpad);
	ret = gst_pad_set_caps(cmplxrfft->srcpad, newcaps);
	gst_caps_unref(newcaps);
	gst_object_unref(cmplxrfft);
	return ret;
}

static void gst_cmplxrfft_class_init(Gst_cmplxrfft_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_cmplxrfft_change_state;

	gst_element_class_set_details(gstelement_class, &cmplxrfft_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_cmplxrfft_init(Gst_cmplxrfft *cmplxrfft)
{
	cmplxrfft->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (cmplxrfft->sinkpad, gst_cmplxrfft_chain);
	gst_element_add_pad(GST_ELEMENT(cmplxrfft), cmplxrfft->sinkpad);

	cmplxrfft->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(cmplxrfft), cmplxrfft->srcpad);

	gst_pad_set_setcaps_function(cmplxrfft->sinkpad, gst_cmplxrfft_setcaps);

	cmplxrfft->buffer = NULL;
	cmplxrfft->length = 512;
	gst_cmplxrfft_setup(cmplxrfft);
	cmplxrfft->offset = 0;
}

GType gst_cmplxrfft_get_type(void)
{
	static GType cmplxrfft_type = 0;

	if (!cmplxrfft_type) {
		static const GTypeInfo cmplxrfft_info = {
			sizeof(Gst_cmplxrfft_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_cmplxrfft_class_init,
			NULL,
			NULL,
			sizeof(Gst_cmplxrfft),
			0,
			(GInstanceInitFunc)gst_cmplxrfft_init,
		};
		cmplxrfft_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstComplexRFFT", &cmplxrfft_info, 0);
	}
	return cmplxrfft_type;
}

