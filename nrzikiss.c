/*
 *	NRZI decoder with KISS output.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 *
 *	The fcs code is based on the crc-ccit and hdlc code from
 *	linux-2.6.10
 */

#include "gstiq.h"
#include <string.h>
#include <math.h>

static GstElementDetails nrzikiss_details = GST_ELEMENT_DETAILS(
	"NRZI AX.25 decoder with KISS output",
	"Codec/Decoder/data",
	"Decodes NRZI AX.25 frames and outputs them in KISS format",
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
		"rate = (int) [ 1, MAX], "
		"channels = (int) 1"
	)
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"application/x-tnc-kiss"
	)
);

static GstElementClass *parent_class = NULL;

static unsigned short crc_ccitt_table[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};


static GstFlowReturn gst_nrzikiss_chain(GstPad *pad, GstBuffer *buf)
{
	Gst_nrzikiss *nrzikiss;
	GstCaps *caps;
	gfloat *demod;
	int i;
	int pushed = 0;

	nrzikiss = GST_NRZIKISS(gst_pad_get_parent(pad));

	demod = (gfloat *)GST_BUFFER_DATA(buf);
	for (i = 0; i < GST_BUFFER_SIZE(buf)/sizeof(gfloat); i ++) {
		int bit;
		if (demod[i] > 0.0) {
			bit = nrzikiss->lastbit;
			nrzikiss->lastbit = 1;
		} else {
			bit = !nrzikiss->lastbit;
			nrzikiss->lastbit = 0;
		}
		if (bit) {
			nrzikiss->ones++;
		} else {
			if (nrzikiss->ones == 5) {
				nrzikiss->ones = 0;
				nrzikiss->stuffed = 3;
				/* Stuffed bit, drop it and go to next */
				continue;
			}
			nrzikiss->ones = 0;
			if (nrzikiss->stuffed >= 0)
				nrzikiss->stuffed--;
		}
		nrzikiss->byte >>= 1;
		nrzikiss->byte |= bit * 128;
		if (nrzikiss->byte == 0x7e && nrzikiss->stuffed < 0) {
			nrzikiss->bytelen = 0;
			if (nrzikiss->buf) {
				if ((nrzikiss->crc & 0xffff) != 0xf0b8 ||
				    GST_BUFFER_SIZE(nrzikiss->buf) < 5) {
					nrzikiss->crc = 0xffff;
					GST_BUFFER_SIZE(nrzikiss->buf) = 2;
					continue;
				}
				GST_BUFFER_SIZE(nrzikiss->buf)-=2;
				GST_BUFFER_DATA(nrzikiss->buf)
				    [GST_BUFFER_SIZE(nrzikiss->buf)++] = 0xc0;
				GST_BUFFER_TIMESTAMP(nrzikiss->buf) =
				    GST_BUFFER_TIMESTAMP(buf);
				GST_BUFFER_OFFSET(nrzikiss->buf) =
				    nrzikiss->offset;
				nrzikiss->offset += 
				    GST_BUFFER_SIZE(nrzikiss->buf);
				caps = gst_pad_get_caps(nrzikiss->srcpad);
				gst_buffer_set_caps(nrzikiss->buf, caps);
				gst_caps_unref(caps);
				gst_pad_push(nrzikiss->srcpad, nrzikiss->buf);
				pushed = 1;
			}
			nrzikiss->buf = gst_buffer_new();
			GST_BUFFER_SIZE(nrzikiss->buf) = 2;
			GST_BUFFER_DATA(nrzikiss->buf) = (guchar *)g_malloc(1024);
			GST_BUFFER_DATA(nrzikiss->buf)[0] = 0xc0;
			GST_BUFFER_DATA(nrzikiss->buf)[1] = 0x0;
			nrzikiss->crc = 0xffff;
		}
		if (++nrzikiss->bytelen == 8) {
			guchar *buffer;

			nrzikiss->bytelen = 0;
			if (!nrzikiss->buf)
				continue;
			if (GST_BUFFER_SIZE(nrzikiss->buf) >= 32768)
				continue;
			if (GST_BUFFER_SIZE(nrzikiss->buf) % 1024 >= 1022) {
				guchar *newbuf;
				newbuf = (guchar *) g_realloc(
				    GST_BUFFER_DATA(nrzikiss->buf),
				    GST_BUFFER_SIZE(nrzikiss->buf) + 1026);
				if (!newbuf) {
					/* Low mem, we drop whatever we had
					   and hope frames are smaller than
					   this (or more mem will come back)
					 */
					nrzikiss->crc = 0xffff;
					GST_BUFFER_SIZE(nrzikiss->buf) = 2;
					continue;
				}
				GST_BUFFER_DATA(nrzikiss->buf) = newbuf;
			}
			buffer = GST_BUFFER_DATA(nrzikiss->buf);
			nrzikiss->crc = (nrzikiss->crc >> 8) ^
			    crc_ccitt_table[(nrzikiss->crc ^ nrzikiss->byte) & 0xff];
			if (nrzikiss->byte == 0xc0) {
				buffer[GST_BUFFER_SIZE(nrzikiss->buf)] = 0xdb;
				buffer[GST_BUFFER_SIZE(nrzikiss->buf)+1] = 0xdc;
				GST_BUFFER_SIZE(nrzikiss->buf)+= 2;
			} else if (nrzikiss->byte == 0xdb) {
				buffer[GST_BUFFER_SIZE(nrzikiss->buf)] = 0xdb;
				buffer[GST_BUFFER_SIZE(nrzikiss->buf)+1] = 0xdd;
				GST_BUFFER_SIZE(nrzikiss->buf)+= 2;
			} else {
				buffer[GST_BUFFER_SIZE(nrzikiss->buf)] =
				    nrzikiss->byte;
				GST_BUFFER_SIZE(nrzikiss->buf)++;
			}
		}
	}
	if (!pushed) {
		GstBuffer *outbuf = gst_buffer_new_and_alloc(1);
		if (outbuf) {
			GST_BUFFER_DATA(outbuf)[0] = 0xc0;
			GST_BUFFER_TIMESTAMP(outbuf) = GST_BUFFER_TIMESTAMP(buf);
			GST_BUFFER_OFFSET(outbuf) = nrzikiss->offset;
			caps = gst_pad_get_caps(nrzikiss->srcpad);
			gst_buffer_set_caps(outbuf, caps);
			gst_caps_unref(caps);
			gst_pad_push(nrzikiss->srcpad, outbuf);
			nrzikiss->offset++;
		}
	}
	gst_buffer_unref(buf);
	return GST_FLOW_OK;
}


static GstStateChangeReturn gst_nrzikiss_change_state(GstElement *element,
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static void gst_nrzikiss_class_init(Gst_nrzikiss_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_nrzikiss_change_state;

	gst_element_class_set_details(gstelement_class, &nrzikiss_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));
}

static void gst_nrzikiss_init(Gst_nrzikiss *nrzikiss)
{
	nrzikiss->sinkpad = gst_pad_new_from_template(
	    gst_static_pad_template_get (&sink_template), "sink");

	gst_pad_set_chain_function (nrzikiss->sinkpad, gst_nrzikiss_chain);
	gst_element_add_pad (GST_ELEMENT(nrzikiss), nrzikiss->sinkpad);

	nrzikiss->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(nrzikiss), nrzikiss->srcpad);

	gst_pad_use_fixed_caps(nrzikiss->srcpad);
	gst_pad_use_fixed_caps(nrzikiss->sinkpad);

	nrzikiss->buf = NULL;
	nrzikiss->byte = 0;
	nrzikiss->bytelen = 0;
	nrzikiss->ones = 0;
	nrzikiss->stuffed = 0;
	nrzikiss->offset = 0;
}

GType gst_nrzikiss_get_type(void)
{
	static GType nrzikiss_type = 0;

	if (!nrzikiss_type) {
		static const GTypeInfo nrzikiss_info = {
			sizeof(Gst_nrzikiss_class),
			NULL,
			NULL,
 			(GClassInitFunc)gst_nrzikiss_class_init,
	 		NULL,
			NULL,
			sizeof(Gst_nrzikiss),
			0,
			(GInstanceInitFunc)gst_nrzikiss_init,
		};
		nrzikiss_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstNRZIKISS", &nrzikiss_info, 0);
	}
	return nrzikiss_type;
}
