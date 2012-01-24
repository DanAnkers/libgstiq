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
#include <complex.h>

static GstElementDetails iqfdemod_details = GST_ELEMENT_DETAILS(
	"Frequency domain demodulator",
	"Filter/Effect/Audio",
	"Frequency domain demodulator",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	ARG_0,
	ARG_OFFSET,
	ARG_MODE,
};

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

static int gst_iqfdemod_setup(Gst_iqfdemod *fdemod)
{
	return 0;
}

static GstFlowReturn gst_iqfdemod_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_iqfdemod *fdemod;
	GstBuffer *outbuf;
	GstCaps *caps;
	int i, j;
	complex float *outdata;
	complex float *indata;
	float factor;
	int len;
	float f1, f2;
	int foffset;

	fdemod = GST_IQFDEMOD(gst_pad_get_parent(pad));
	caps = gst_pad_get_caps(fdemod->srcpad);
	if (!gst_caps_is_fixed(caps)) {
		GstStructure *structure;
		gst_caps_unref(caps);
		caps = gst_pad_get_allowed_caps(fdemod->srcpad);
		structure = gst_caps_get_structure(caps, 0);
		gst_structure_get_int(structure, "rate", &fdemod->srcrate);
		gst_structure_get_int(structure, "length", &fdemod->srclength);
		gst_iqfdemod_setup(fdemod);
	}

	outbuf = gst_buffer_new_and_alloc(fdemod->srclength*sizeof(float) * 2);
	if (!outbuf)
		goto out;
	outdata = (complex float *)GST_BUFFER_DATA(outbuf);
	indata = (complex float *)GST_BUFFER_DATA(buf);

	for (i = 0; i < fdemod->srclength; i++)
		outdata[i] = 0.0;
	
	factor = 1.0;
	
	foffset = fdemod->foffset / (fdemod->srcrate / fdemod->srclength);
	switch (fdemod->mode) {
		case FDEMOD_RAW:
			for (i = -(fdemod->srclength/2), 
			    j = foffset -(fdemod->srclength/2);
			     i < fdemod->srclength/2; i++, j++) {
				if (j < -(fdemod->sinklength)/2)
					continue;
				if (j >= fdemod->sinklength/2)
					continue;
				if (i >= 0 && j >= 0)
					outdata[i] = indata[j] * factor;
				if (i >= 0 && j < 0)
					outdata[i] =
					    indata[j + fdemod->sinklength] * factor;
				if (i < 0 && j >= 0)
					outdata[i + fdemod->srclength] =
					    indata[j] * factor;
				if (i < 0 && j < 0)
					outdata[i + fdemod->srclength] =
					    indata[j + fdemod->sinklength] * factor;
			}
			break;
		case FDEMOD_USB:
			outdata[0] = 0.0 + 0.0 * I;
			outdata[fdemod->srclength-1] = 0.0 + 0.0 * I;
			f1 = 300.0 / (fdemod->srcrate/fdemod->srclength);
			f2 = 3000.0 / (fdemod->srcrate/fdemod->srclength);
			len = fdemod->srclength/2;
			for (i = 1, j = foffset + 1; i < len; i++, j++) {
				factor = 2.0
				    * (float)i / (f1+(float)(i))
				    * ((float)(len-i)) / (f2+(float)(len-i));
				if (j < -(fdemod->sinklength)/2)
					continue;
				if (j >= fdemod->sinklength/2)
					continue;
				if (j >= 0)
					outdata[i] = indata[j] * factor;
				else
					outdata[i] =
					    indata[j + fdemod->sinklength] * factor;
				*(float *)&outdata[len+len-i] = *(float *)&outdata[i];
				*((float *)(&outdata[len+len-i])+1) = -*((float *)(&outdata[i])+1);
			}
			break;
		case FDEMOD_LSB:
			outdata[0] = 0.0 + 0.0 * I;
			outdata[fdemod->srclength-1] = 0.0 + 0.0 * I;
			f1 = 300.0 / (fdemod->srcrate/fdemod->srclength);
			f2 = 3000.0 / (fdemod->srcrate/fdemod->srclength);
			len = fdemod->srclength/2;
			for (i = 1, j = foffset - 1; i < len; i++, j--) {
				factor = 2.0
				    * (float)i / (f1+(float)(i))
				    * ((float)(len-i)) / (f2+(float)(len-i));
				if (j < -(fdemod->sinklength)/2)
					continue;
				if (j >= fdemod->sinklength/2)
					continue;
				if (j >= 0)
					outdata[len+len-i] = indata[j] * factor;
				else
					outdata[len+len-i] =
					    indata[j + fdemod->sinklength] * factor;
				*(float *)&outdata[i] = *(float *)&outdata[len+len-i];
				*((float *)(&outdata[i])+1) = -*((float *)(&outdata[len+len-i])+1);
			}
			break;
		default:
			for (i = 0; i < fdemod->srclength; i++)
				outdata[i] = 0.0 + 0.0 * I;
	}


	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
	fdemod->offset += fdemod->srclength;
	GST_BUFFER_OFFSET(outbuf) = fdemod->offset;
	gst_pad_push(fdemod->srcpad, outbuf);
out:
	gst_buffer_unref(buf);
	gst_object_unref(fdemod);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_iqfdemod_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_iqfdemod_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_iqfdemod *fdemod;
	GstStructure *structure;
	gboolean ret = TRUE;
	int length;
	int rate;

	fdemod = GST_IQFDEMOD(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);

	gst_structure_get_int(structure, "length", &length);
	gst_structure_get_int(structure, "rate", &rate);
	if (rate && length) {
		if (pad == fdemod->srcpad) {
			fdemod->srclength = length;
			fdemod->srcrate = rate;
		} else {
			fdemod->sinklength = length;
			fdemod->sinkrate = rate;
		}
	} else {
		ret = FALSE;
	}
	if (ret == TRUE)
		gst_iqfdemod_setup(fdemod);

	gst_object_unref(fdemod);
	return ret;
}

static void gst_iqfdemod_set_property(GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
	Gst_iqfdemod *fdemod;

	g_return_if_fail(GST_IS_IQFDEMOD(object));
	fdemod = GST_IQFDEMOD(object);

	switch(prop_id) {
		case ARG_OFFSET:
			fdemod->foffset = g_value_get_int(value);
			break;
		case ARG_MODE:
			fdemod->mode = g_value_get_int(value);
			break;
		default:
			break;
	}
}

static void gst_iqfdemod_get_property(GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
	Gst_iqfdemod *fdemod;

	g_return_if_fail(GST_IS_IQFDEMOD(object));
	fdemod = GST_IQFDEMOD(object);

	switch(prop_id) {
		case ARG_OFFSET:
			g_value_set_int(value, fdemod->foffset);
			break;
		case ARG_MODE:
			g_value_set_int(value, fdemod->mode);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void gst_iqfdemod_class_init(Gst_iqfdemod_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gobject_class->set_property = gst_iqfdemod_set_property;
	gobject_class->get_property = gst_iqfdemod_get_property;

	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_OFFSET,
	    g_param_spec_int("offset", "offset", "offset",
	         -G_MAXINT, G_MAXINT, 0, G_PARAM_READWRITE));
	g_object_class_install_property(G_OBJECT_CLASS (klass), ARG_MODE,
	    g_param_spec_int("mode", "mode", "mode",
	         0, G_MAXINT, 0, G_PARAM_READWRITE));

	gstelement_class->change_state = gst_iqfdemod_change_state;

	gst_element_class_set_details(gstelement_class, &iqfdemod_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_iqfdemod_init(Gst_iqfdemod *fdemod)
{
	fdemod->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (fdemod->sinkpad, gst_iqfdemod_chain);
	gst_element_add_pad(GST_ELEMENT(fdemod), fdemod->sinkpad);

	fdemod->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(fdemod), fdemod->srcpad);

	gst_pad_set_setcaps_function(fdemod->sinkpad, gst_iqfdemod_setcaps);
	gst_pad_set_setcaps_function(fdemod->srcpad, gst_iqfdemod_setcaps);
	gst_pad_use_fixed_caps(fdemod->sinkpad);
	gst_pad_use_fixed_caps(fdemod->srcpad);

//	cmplxrfft->buffer = NULL;
//	cmplxrfft->length = 512;
	fdemod->sinklength = 0;
	fdemod->srclength = 0;
	fdemod->sinkrate = 0;
	fdemod->srcrate = 0;
	fdemod->mode = FDEMOD_USB;
	gst_iqfdemod_setup(fdemod);
	fdemod->foffset = 0;
	fdemod->offset = 0;
}

GType gst_iqfdemod_get_type(void)
{
	static GType iqfdemod_type = 0;

	if (!iqfdemod_type) {
		static const GTypeInfo iqfdemod_info = {
			sizeof(Gst_iqfdemod_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_iqfdemod_class_init,
			NULL,
			NULL,
			sizeof(Gst_iqfdemod),
			0,
			(GInstanceInitFunc)gst_iqfdemod_init,
		};
		iqfdemod_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstIQFDem", &iqfdemod_info, 0);
	}
	return iqfdemod_type;
}

