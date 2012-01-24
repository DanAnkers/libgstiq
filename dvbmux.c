/*
 *	DVB transport stream multiplexer.
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

static GstElementDetails dvbmux_details = GST_ELEMENT_DETAILS(
	"DVB Multiplexer",
	"Codec/Muxer",
	"Multiplexes MPEG2 streams to DVB transport streams",
	"Jeroen Vreeken (pe1rxq@amsat.org)"
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
	"src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		"video/mpeg, "
		"mpegversion = (int) 2, "
		"systemstream = (bool) true, "
		"rate = (int) [ 64, MAX ]"
	)
);

static GstStaticPadTemplate sink_video_template = GST_STATIC_PAD_TEMPLATE(
	"video_%d",
	GST_PAD_SINK,
	GST_PAD_REQUEST,
	GST_STATIC_CAPS(
		"video/mpeg, "
		"mpegversion = (int) 2, "
		"systemstream = (bool) false"
	)
);

static GstStaticPadTemplate sink_audio_template = GST_STATIC_PAD_TEMPLATE(
	"audio_%d",
	GST_PAD_SINK,
	GST_PAD_REQUEST,
	GST_STATIC_CAPS(
		"audio/mpeg, "
		"mpegversion = (int) 1, "
		"systemstream = (bool) false"
	)
);

static GstElementClass *parent_class = NULL;

static void dvbmux_pat_fill(Gst_dvbmux *dvbmux)
{
	int patlen;
	struct dvb_ts_af *af;
	struct dvb_ts_pat *pat;
	struct dvb_ts_pat_prog *prog;
	unsigned long *crc;
	int i;

	patlen = sizeof(struct dvb_ts_pat);
	//todo: add 'network' channel???
	patlen += sizeof(struct dvb_ts_pat_prog) * dvbmux->nrprograms;
	patlen += sizeof(unsigned long);

	af = (struct dvb_ts_af *)dvbmux->pat.pl;
	pat = (struct dvb_ts_pat *)&dvbmux->pat.pl[184 - patlen];
	prog = (struct dvb_ts_pat_prog *)(pat+1);

	dvbmux->pat.sync_byte = DVBMUX_SYNC;
	dvbmux->pat.pid = DVBMUX_PID_PAT;
	dvbmux->pat.control = DVBMUX_AF_AF | DVBMUX_AF_PL;

	af->len = 184 - 1 - patlen;
	af->flags = 0;

	pat->pointer = 0;
//	pat->table_id = 0;
//	pat->section = ;
//	pat->ts_id = ;
//	pat->version = ;
//	pat->sectionnr = ;
//	pat->lastsection = ;

	for (i = 0; i < dvbmux->nrprograms; i++) {
		prog->number = i + 1;
		prog->pid = dvbmux->programs[i]->pmt_pid;
		prog++;
	}

	crc = (unsigned long *)prog;
//	crc
}

static void gst_dvbmux_loop(GstPad *pad)
{
	GstBuffer *outbuf;
	Gst_dvbmux *dvbmux;
return;
	dvbmux = GST_DVBMUX(GST_PAD_PARENT(pad));

	dvbmux_pat_fill(dvbmux);

	outbuf = gst_buffer_new_and_alloc(188);
	gst_buffer_set_caps (outbuf, GST_PAD_CAPS(dvbmux->srcpad));
	memcpy(GST_BUFFER_DATA(outbuf), &dvbmux->pat, 188);
//GST_BUFFER_TIMESTAMP (outbuf) = adder->timestamp;
//GST_BUFFER_OFFSET (outbuf) = adder->offset;
//    GST_BUFFER_DURATION (outbuf) = adder->timestamp -
//        GST_BUFFER_TIMESTAMP (outbuf);
	gst_pad_push(dvbmux->srcpad, outbuf);
}

static GstFlowReturn gst_dvbmux_chain(GstPad *pad, GstBuffer *buf)
{
	int i;

	printf("pad: %p\t", pad);
	printf("data in: %d\t", GST_BUFFER_SIZE(buf));
	printf("timestamp: %lld\n", GST_BUFFER_TIMESTAMP(buf));

	for (i = 0; i < 16; i++) {
		printf("%02x  ", GST_BUFFER_DATA(buf)[i]);
	}
	printf("\n");

	gst_buffer_unref(buf);
	return GST_FLOW_OK;
}

static gboolean gst_dvbmux_actpush(GstPad *pad, gboolean active)
{
	gboolean result = FALSE;
	Gst_dvbmux *dvbmux;

	dvbmux = GST_DVBMUX(gst_pad_get_parent(pad));

	//todo lock mutex here
	if (active) {
		dvbmux->state = GST_FLOW_OK;
		if (gst_pad_is_linked(pad))
			result = gst_pad_start_task(pad, 
			     (GstTaskFunction)gst_dvbmux_loop, pad);
		else
			result = TRUE;
	} else {
		dvbmux->state = GST_FLOW_WRONG_STATE;
		gst_pad_stop_task(pad);
	}
	//todo unlock mutex here

	gst_object_unref(dvbmux);

	return result;
}

static GstPadLinkReturn gst_dvbmux_link_src(GstPad *pad, GstPad *peer)
{
	GstPadLinkReturn result = GST_PAD_LINK_OK;
	Gst_dvbmux *dvbmux;

	dvbmux = GST_DVBMUX(gst_pad_get_parent(pad));

	if (GST_PAD_LINKFUNC(peer)) {
		result = GST_PAD_LINKFUNC(peer)(peer, pad);
	}

	if (GST_PAD_LINK_SUCCESSFUL(result)) {
		//todo lock mutex
		if (dvbmux->state == GST_FLOW_OK) {
			gst_pad_start_task(pad, 
			    (GstTaskFunction)gst_dvbmux_loop, pad);
		}
		//todo unlock mutex
	}
	gst_object_unref(dvbmux);

	return result;
}

static gboolean gst_dvbmux_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_dvbmux *dvbmux;
	GstStructure *structure;
	const GValue *pps;

	dvbmux = GST_DVBMUX(gst_pad_get_parent(pad));
	structure = gst_caps_get_structure(caps, 0);
	pps = gst_structure_get_value(structure, "rate");
	if (!GST_VALUE_HOLDS_FRACTION(pps))
		return FALSE;

	dvbmux->rate = gst_value_get_fraction_numerator(pps);
	dvbmux->rate_base = gst_value_get_fraction_denominator(pps);

	return TRUE;
}

static gboolean gst_dvbmux_video_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_dvbmux *dvbmux;

	dvbmux = GST_DVBMUX(gst_pad_get_parent(pad));

	return TRUE;
}

static gboolean gst_dvbmux_audio_setcaps(GstPad *pad, GstCaps *caps)
{
	Gst_dvbmux *dvbmux;

	dvbmux = GST_DVBMUX(gst_pad_get_parent(pad));

	return TRUE;
}

struct dvbmux_program *gst_dvbmux_new_program(Gst_dvbmux *dvbmux)
{
	struct dvbmux_program **progs;
	struct dvbmux_program *prog;

	prog = malloc(sizeof(struct dvbmux_program));
	if (!prog)
		return NULL;

	progs = realloc(dvbmux->programs,
	    sizeof(struct dvbmux_program*) * (dvbmux->nrprograms + 1));
	if (!progs) {
		free(prog);
		return NULL;
	}
	prog->videosink = NULL;
	prog->audiosink = NULL;
	prog->prog = dvbmux->nrprograms + 1;
	prog->pmt_pid = prog->prog * 0x100;
	prog->video_pid = prog->prog * 0x100 + 1;
	prog->audio_pid = prog->prog * 0x100 + 2;
	progs[dvbmux->nrprograms] = prog;
	dvbmux->programs = progs;
	dvbmux->nrprograms++;

	return prog;
}

static GstPad *gst_dvbmux_request_new_video_pad(Gst_dvbmux *dvbmux,
    GstPadTemplate *templ, const gchar *rname)
{
	int prognr;
	gchar *name;
	GstPad *newpad;

	for (prognr = 0; prognr < dvbmux->nrprograms; prognr++)
		if (dvbmux->programs[prognr]->videosink == NULL)
			break;
	if (prognr == dvbmux->nrprograms)
		if (gst_dvbmux_new_program(dvbmux) == NULL)
			return NULL;

	name = g_strdup_printf("video_%d", prognr);
	newpad = gst_pad_new_from_template (templ, name);

	gst_pad_set_getcaps_function(newpad, gst_pad_proxy_getcaps);
	gst_pad_set_setcaps_function(newpad, gst_dvbmux_video_setcaps);
	if (!gst_element_add_pad(GST_ELEMENT(dvbmux), newpad))
		goto could_not_add;

	dvbmux->programs[prognr]->videosink = newpad;

	gst_pad_set_chain_function (newpad, gst_dvbmux_chain);

	printf("video pad: %p\n", newpad);
	return newpad;

could_not_add:
	gst_object_unref(newpad);
	return NULL;
}

static GstPad *gst_dvbmux_request_new_audio_pad(Gst_dvbmux *dvbmux,
    GstPadTemplate *templ, const gchar *rname)
{
	int prognr;
	gchar *name;
	GstPad *newpad;

	for (prognr = 0; prognr < dvbmux->nrprograms; prognr++)
		if (dvbmux->programs[prognr]->audiosink == NULL)
			break;
	if (prognr == dvbmux->nrprograms)
		if (gst_dvbmux_new_program(dvbmux) == NULL)
			return NULL;

	name = g_strdup_printf("audio_%d", prognr);
	newpad = gst_pad_new_from_template (templ, name);

	gst_pad_set_getcaps_function(newpad, gst_pad_proxy_getcaps);
	gst_pad_set_setcaps_function(newpad, gst_dvbmux_audio_setcaps);
	if (!gst_element_add_pad(GST_ELEMENT(dvbmux), newpad))
		goto could_not_add;

	dvbmux->programs[prognr]->audiosink = newpad;

	gst_pad_set_chain_function (newpad, gst_dvbmux_chain);

	printf("audio pad: %p\n", newpad);
	return newpad;

could_not_add:
	gst_object_unref(newpad);
	return NULL;
}

static GstPad *gst_dvbmux_request_new_pad(GstElement *element,
    GstPadTemplate *templ, const gchar *rname)
{
	Gst_dvbmux *dvbmux;

	dvbmux = GST_DVBMUX(element);

	printf("Requested name: %s\n", rname);
	if (!strncmp("video", templ->name_template, 5)) {
		printf("video template\n");
		return gst_dvbmux_request_new_video_pad(dvbmux, templ, rname);
	}
	if (!strncmp("audio", templ->name_template, 5)) {
		printf("audio template\n");
		return gst_dvbmux_request_new_audio_pad(dvbmux, templ, rname);
	}

	return NULL;
}

static GstStateChangeReturn gst_dvbmux_change_state(GstElement *element, 
    GstStateChange transition)
{
	return parent_class->change_state(element, transition);
}

static void gst_dvbmux_class_init(Gst_dvbmux_class *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

	gstelement_class->change_state = gst_dvbmux_change_state;

	gst_element_class_set_details(gstelement_class, &dvbmux_details);

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&src_template));

	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_video_template));
	gst_element_class_add_pad_template(gstelement_class,
	    gst_static_pad_template_get(&sink_audio_template));

	gstelement_class->request_new_pad = gst_dvbmux_request_new_pad;
}

static void gst_dvbmux_init(Gst_dvbmux *dvbmux)
{
	dvbmux->srcpad = gst_pad_new_from_template(
	    gst_static_pad_template_get(&src_template), "src");
	gst_element_add_pad(GST_ELEMENT(dvbmux), dvbmux->srcpad);

	gst_pad_set_activatepush_function(dvbmux->srcpad, gst_dvbmux_actpush);
	gst_pad_set_link_function(dvbmux->srcpad, gst_dvbmux_link_src);
	gst_pad_set_setcaps_function(dvbmux->srcpad, gst_dvbmux_setcaps);

	dvbmux->nrprograms = 0;
	dvbmux->programs = NULL;
}

GType gst_dvbmux_get_type(void)
{
	static GType dvbmux_type = 0;

	if (!dvbmux_type) {
		static const GTypeInfo dvbmux_info = {
			sizeof(Gst_dvbmux_class),
			NULL,
			NULL,
			(GClassInitFunc)gst_dvbmux_class_init,
			NULL,
			NULL,
			sizeof(Gst_dvbmux),
			0,
			(GInstanceInitFunc)gst_dvbmux_init,
		};
		dvbmux_type = g_type_register_static(GST_TYPE_ELEMENT,
		    "GstDVBMUX", &dvbmux_info, 0);
	}
	return dvbmux_type;
}
