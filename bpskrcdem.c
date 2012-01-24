/*
 *	BPSK-RC demodulator
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

static GstElementDetails bpskrcdem_details = GST_ELEMENT_DETAILS(
	"BPSK-RC demodulator plugin",
	"Codec/Decoder/Data",
	"Demodulates a BPSK-RC signal in polar IQ format",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

enum {
	STATE_LOW,
	STATE_HIGH,
	STATE_HIGHBIT,
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"audio/x-polar-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 64, "
		"rate = (int) [ 1, MAX], "
		"channels = (int) 1"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-raw-float, "
		"endianness = (int) BYTE_ORDER, "
		"depth = (int) 32, "
		"rate = (int) [ 1, MAX], "
		"channels = (int) 1"
	)
);

static GstElementClass *parent_class = NULL;

static GstFlowReturn gst_bpskrcdem_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_bpskrcdem *bpskrcdem;
	GstCaps *caps;
	GstBuffer *outbuf;
	gfloat *iqbuf, *bufout, abs, angle;
	int i, j;

	bpskrcdem = GST_BPSKRCDEM(gst_pad_get_parent(pad));

	outbuf = gst_buffer_new();
	GST_BUFFER_SIZE(outbuf) = 0;
	GST_BUFFER_DATA(outbuf) = g_malloc(GST_BUFFER_SIZE(buf));
	bufout = (gfloat *)GST_BUFFER_DATA(outbuf);
	caps = gst_pad_get_caps(bpskrcdem->srcpad);
	if (!gst_caps_is_fixed(caps)) {
		GstStructure *structure;
		gst_caps_unref(caps);
		caps = gst_pad_get_allowed_caps(bpskrcdem->srcpad);
		structure = gst_caps_get_structure(caps, 0);
		gst_structure_get_int(structure, "rate", &bpskrcdem->symbolrate);
		bpskrcdem->symbollen =
		    (float)bpskrcdem->rate/(float)bpskrcdem->symbolrate;
	}
	gst_buffer_set_caps(outbuf, caps);
	gst_caps_unref(caps);

	iqbuf = (gfloat *)GST_BUFFER_DATA(buf);
	j = 0;
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(gfloat); i += 2) {
		abs = iqbuf[i];
		angle = iqbuf[i+1];

		if (abs > bpskrcdem->avrg) {
			bpskrcdem->avrg +=
			    (abs - bpskrcdem->avrg)/(bpskrcdem->symbollen);
		} else {
			bpskrcdem->avrg +=
			    (abs - bpskrcdem->avrg)/(bpskrcdem->symbollen * 64);
		}

		switch(bpskrcdem->state) {
			case STATE_LOW:
				if (abs > bpskrcdem->avrg * 0.71) {
					bpskrcdem->state = STATE_HIGH;
					if (bpskrcdem->count>(bpskrcdem->symbollen/2))
						bpskrcdem->count -= bpskrcdem->symbollen;
					bpskrcdem->count /= 2;
				}
				bpskrcdem->count++;
				if (bpskrcdem->count==bpskrcdem->symbollen/4)
					bpskrcdem->state=STATE_HIGHBIT;
				break;
			case STATE_HIGH:
				if (bpskrcdem->count>=(bpskrcdem->symbollen/4) &&
				    bpskrcdem->count<(bpskrcdem->symbollen/2)) {
					float devangle;

					devangle = angle - bpskrcdem->prevangle;
					bpskrcdem->prevangle = angle;
					if (devangle > M_PI)
						devangle -= 2 * M_PI;
					if (devangle < -M_PI)
						devangle += 2 * M_PI;
					if (devangle < 0)
						devangle = -devangle;
					if((devangle >= M_PI_2 &&
					    devangle < M_PI_2 * 3))
						bpskrcdem->bit *= -1;
					bufout[j] = bpskrcdem->bit;
					j++;
					bpskrcdem->state = STATE_HIGHBIT;
				}
			case STATE_HIGHBIT:
				bpskrcdem->count++;
				if (bpskrcdem->count > bpskrcdem->symbollen)
					bpskrcdem->count -= bpskrcdem->symbollen;
				if (abs < bpskrcdem->avrg * 0.69 &&
				    bpskrcdem->count > bpskrcdem->symbollen/4+1)
					bpskrcdem->state = STATE_LOW;
				if (bpskrcdem->count>=(bpskrcdem->symbollen/4)*3) {
					bpskrcdem->state = STATE_HIGH;
				}
				break;
		}
	}
	GST_BUFFER_SIZE(outbuf) = j * sizeof(gfloat);
	GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
	GST_BUFFER_OFFSET(outbuf) = bpskrcdem->offset;
	bpskrcdem->offset += j;
	gst_buffer_unref(buf);
	if (j == 0)
		gst_buffer_unref(outbuf);
	else
		gst_pad_push(bpskrcdem->srcpad, outbuf);
	return GST_FLOW_OK;
}

static GstStateChangeReturn gst_bpskrcdem_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static gboolean gst_bpskrcdem_setcaps(GstPad *pad, GstCaps *caps)
{
	GstStructure *structure;
	Gst_bpskrcdem *bpskrcdem;

	bpskrcdem = GST_BPSKRCDEM(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	gst_structure_get_int(structure, "rate", &bpskrcdem->rate);
	gst_pad_use_fixed_caps(pad);

	return TRUE;
}

static void gst_bpskrcdem_class_init(Gst_bpskrcdem_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_bpskrcdem_change_state;

	gst_element_class_set_details(gstelement_class, &bpskrcdem_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_bpskrcdem_init(Gst_bpskrcdem *bpskrcdem)
{
	bpskrcdem->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (bpskrcdem->sinkpad, gst_bpskrcdem_chain);
	gst_element_add_pad (GST_ELEMENT(bpskrcdem), bpskrcdem->sinkpad);

	bpskrcdem->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(bpskrcdem), bpskrcdem->srcpad);

	gst_pad_set_setcaps_function(bpskrcdem->sinkpad, gst_bpskrcdem_setcaps);

	bpskrcdem->avrg = 0.0;
	bpskrcdem->symbolrate = 1;
	bpskrcdem->prevangle = 0.0;
	bpskrcdem->bit = -1.0;
	bpskrcdem->state = STATE_LOW;
	bpskrcdem->offset = 0;
}

GType gst_bpskrcdem_get_type(void)
{
	static GType bpskrcdem_type = 0;

	if (! bpskrcdem_type) {
		static const GTypeInfo bpskrcdem_info = {
			sizeof(Gst_bpskrcdem_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_bpskrcdem_class_init,
			NULL,
			NULL,
			sizeof(Gst_bpskrcdem),
			0,
			(GInstanceInitFunc)gst_bpskrcdem_init,
		};
		bpskrcdem_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstBPSKrcDem", &bpskrcdem_info, 0);
	}
	return bpskrcdem_type;
}
