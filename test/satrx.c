/*
 *	satrx.c
 *
 *	A BPSK-RC satellite demodulator.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#include <stdlib.h>
#include <unistd.h>
#include <gst/gst.h>

static void event_loop(GstElement * pipe)
{
	GstBus *bus;
	GstMessageType revent;
	GstMessage *message = NULL;

	bus = gst_element_get_bus(GST_ELEMENT (pipe));

	while (TRUE) {
	  /* FIXME: Don't use gst_bus_poll */
		message = gst_bus_poll(bus, GST_MESSAGE_ANY, -1);
		g_assert(message != NULL);

    revent = message->type;
		switch (revent) {
			case GST_MESSAGE_EOS:
				gst_message_unref(message);
				return;
			case GST_MESSAGE_WARNING:
			case GST_MESSAGE_ERROR:{
				GError *gerror;
				gchar *debug;

				gst_message_parse_error(message, &gerror, &debug);
				gst_object_default_error(
				    GST_MESSAGE_SRC(message), gerror, debug);
				gst_message_unref(message);
				g_error_free(gerror);
				g_free(debug);
				return;
			}
			default:
				gst_message_unref(message);
				break;
		}
	}
}

static void new_pad(GstElement *element, GstPad *pad, GstElement *other)
{
	gst_pad_link(pad, gst_element_get_pad(other, "sink"));
}

struct afc_context {
	GstElement *fshift;
	GstElement *waterfall;
	int cnt;
};

static gboolean new_afc_value(GstPad *pad, GstBuffer *buffer,
    struct afc_context *context)
{
	gfloat *afc = (gfloat *)GST_BUFFER_DATA(buffer);

	g_object_set(G_OBJECT(context->waterfall), "marker", *afc, NULL);
	g_object_set(G_OBJECT(context->fshift), "shift", -*afc, NULL);
	if (context->cnt++ % 10 == 0)
		fprintf(stderr, "AFC: %f\n", *afc);

	return TRUE;
}

static gboolean new_kiss_data(GstPad *pad, GstBuffer *buffer, gpointer nop)
{
	unsigned char *data = GST_BUFFER_DATA(buffer);

	if (GST_BUFFER_SIZE(buffer) > 1)
		fprintf(stderr, "DECODED VALID DATA: %d\n",
		    GST_BUFFER_SIZE(buffer));
	write(1, data, GST_BUFFER_SIZE(buffer));
	return TRUE;
}

int main (int argc, char **argv)
{
	GstElement *bin, *filesrc, *decoder, *aconvin, *cmplxin, *tee;
	GstElement *cmplxfft, *teefft;
	GstElement *waterfall, *imagesink;
	GstElement *fshift, *filter, *teecor;
	GstElement *afc, *fakesink;
	GstElement *queue1, *queue2, *queue3, *queue4, *queue5, *queue6;
	GstElement *cmplxout, *aconvout, *audiosink;
	GstElement *polar, *bpskrcdem, *nrzikiss, *kisssink;
	GstCaps *caps;
	struct afc_context afc_context;

	gst_init (&argc, &argv);

	if (argc != 2) {
		g_print("No input file!\n");
		g_print("usage: %s <input file>\n", argv[0]);
		g_print("\n");
		g_print("Decoded KISS data will be send to stdout.\n");
		g_print("Debugging information will be send to stderr.\n");
		g_print("\n");
		exit (-1);
	}

	bin = gst_pipeline_new ("pipeline");
	g_assert (bin);

	filesrc = gst_element_factory_make("filesrc", "disk_source");
	g_assert(filesrc);
	g_object_set(G_OBJECT (filesrc), "location", argv[1], NULL);
	decoder = gst_element_factory_make ("wavparse", "decode");
	g_assert(decoder);
	aconvin = gst_element_factory_make("audioconvert", "aconvin");
	g_assert(aconvin);
 	cmplxin = gst_element_factory_make("iqcmplx", "cmplxin");
	g_assert(cmplxin);
	tee = gst_element_factory_make("tee", "teein");
	g_assert(tee);

	cmplxfft = gst_element_factory_make("cmplxfft", "cmplxfft");
	g_assert(cmplxfft);
	teefft = gst_element_factory_make("tee", "teefft");
	g_assert(teefft);
	queue1 = gst_element_factory_make("queue", "queue1");
	g_assert(queue1);
	queue2 = gst_element_factory_make("queue", "queue2");
	g_assert(queue2);
	queue3 = gst_element_factory_make("queue", "queue3");
	g_assert(queue3);
	queue4 = gst_element_factory_make("queue", "queue4");
	g_assert(queue4);
	queue5 = gst_element_factory_make("queue", "queue5");
	g_assert(queue5);
	queue6 = gst_element_factory_make("queue", "queue6");
	g_assert(queue6);

	waterfall = gst_element_factory_make("waterfall", "waterfall");
	g_assert(waterfall);
	imagesink = gst_element_factory_make("xvimagesink", "imagesink");
	g_assert(imagesink);

	afc = gst_element_factory_make("afc", "afc");
	g_assert(afc);
	fakesink = gst_element_factory_make("fakesink", "fakesink");
	g_assert(fakesink);
	
	fshift = gst_element_factory_make("iqfshift", "fshift");
	g_assert(fshift);
	filter = gst_element_factory_make("firblock", "filter");
	g_assert(filter);
	g_object_set(G_OBJECT(filter), "frequency", 1300, NULL);
	g_object_set(G_OBJECT(filter), "depth", 3, NULL);
	cmplxout = gst_element_factory_make("iqcmplx", "cmplxout");
	g_assert(cmplxout);
	teecor = gst_element_factory_make("tee", "teecor");
	g_assert(teecor);
	polar = gst_element_factory_make("iqpolar", "polar");
	g_assert(polar);
	bpskrcdem = gst_element_factory_make("bpskrcdem", "bpskrcdem");
	g_assert(bpskrcdem);
	nrzikiss = gst_element_factory_make("nrzikiss", "nrzikiss");
	g_assert(nrzikiss);
	kisssink = gst_element_factory_make("fakesink", "kisssink");
	g_assert(kisssink);
 
	aconvout = gst_element_factory_make("audioconvert", "aconvout");
	g_assert(aconvout);
	audiosink = gst_element_factory_make ("osssink", "play_audio");
	g_assert (audiosink);

	gst_bin_add_many (GST_BIN (bin), filesrc, decoder, aconvin, cmplxin,
	    tee, queue1, queue2, queue4,
	    cmplxfft, teefft,
	    queue3, waterfall, imagesink,
	    afc, fakesink,
	    fshift, filter, teecor,
	    queue5, polar, bpskrcdem, nrzikiss, kisssink,
	    queue6, cmplxout, aconvout, audiosink,
	    NULL);

	gst_element_link(filesrc, decoder);
	g_signal_connect(decoder, "pad-added", G_CALLBACK(new_pad), aconvin);
	gst_element_link_many(aconvin, cmplxin, tee, NULL);
	gst_element_link_many(tee, queue1, fshift, filter, teecor, NULL);
	gst_element_link_many(teecor, queue5, polar, bpskrcdem, NULL);
	caps = gst_caps_new_simple("application/x-raw-float",
	    "rate", G_TYPE_INT, 1200, NULL);
	gst_element_link_filtered(bpskrcdem, nrzikiss, caps);
	gst_caps_unref(caps);
	gst_element_link_many(nrzikiss, kisssink, NULL);
	gst_element_link_many(teecor, queue6, cmplxout, aconvout, audiosink, NULL);
	gst_element_link_many(tee, queue2, cmplxfft, teefft, NULL);
	gst_element_link_many(teefft, queue3, waterfall, imagesink, NULL);
	gst_element_link_many(teefft, queue4, afc, fakesink, NULL);

	afc_context.fshift = fshift;
	afc_context.waterfall = waterfall;
	afc_context.cnt = 0;
	gst_pad_add_buffer_probe(gst_element_get_pad(afc, "src"),
	    G_CALLBACK(new_afc_value), &afc_context);
	gst_pad_add_buffer_probe(gst_element_get_pad(kisssink, "sink"),
	    G_CALLBACK(new_kiss_data), NULL);

	gst_element_set_state (bin, GST_STATE_PLAYING);
	event_loop (bin);
	gst_element_set_state (bin, GST_STATE_NULL);
	fprintf(stderr, "Done\n");

	exit (0);
}
