/*
 *	softrx.c
 *
 *	A simple software radio application using libgstiq elements.
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
#include <string.h>
#include <gst/gst.h>

static void event_loop(GstElement * pipe)
{
	GstBus *bus;
	GstMessageType revent;
	GstMessage *message = NULL;

	bus = gst_element_get_bus(GST_ELEMENT (pipe));

	while (TRUE) {
		revent = gst_bus_poll(bus, GST_MESSAGE_ANY, -1);

		message = gst_bus_pop(bus);
		g_assert(message != NULL);

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

int main (int argc, char **argv)
{
	GstElement *bin, *filesrc, *decoder, *aconvin, *cmplxin, *tee;
	GstElement *filter, *filter2, *polar, *demod, *aconvout, *audiosink;
	GstElement *cmplxfft, *waterfall, *imagesink;
	GstElement *queue1, *queue2;

	gst_init (&argc, &argv);

	if (argc < 3) {
		printf("%s: A simple software radio\n", argv[0]);
		printf("Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005\n");
		printf("\n");
		printf("Usage: %s [input] [demod]\n", argv[0]);
		printf("\n");
		printf("demod: Either 'am' or 'fm'\n");
		printf("input: wav filename with IQ signal\n");
		printf("\n");
		return 1;
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

	cmplxfft = gst_element_factory_make("cmplxfft", "cmplxfft");
	g_assert(cmplxfft);
	tee = gst_element_factory_make("tee", "tee");
	g_assert(tee);
	queue1 = gst_element_factory_make("queue", "queue1");
	g_assert(queue1);
	queue2 = gst_element_factory_make("queue", "queue2");
	g_assert(queue2);

	waterfall = gst_element_factory_make("waterfall", "waterfall");
	g_assert(waterfall);
	imagesink = gst_element_factory_make("xvimagesink", "imagesink");
	g_assert(imagesink);

	filter = gst_element_factory_make("firblock", "filter");
	g_assert(filter);
	g_object_set(G_OBJECT(filter), "frequency", 6000, NULL);
	g_object_set(G_OBJECT(filter), "depth", 2, NULL);
	filter2 = gst_element_factory_make("firblock", "filter2");
	g_assert(filter2);
	g_object_set(G_OBJECT(filter2), "frequency", 4000, NULL);
	g_object_set(G_OBJECT(filter2), "depth", 2, NULL);

	polar = gst_element_factory_make("iqpolar", "polar");
	g_assert(polar);

	if (!strcmp(argv[2], "fm")) {
		printf("Using FM demodulator\n");
		demod = gst_element_factory_make("iqfmdem", "demod");
	} else {
		printf("Using AM demodulator\n");
		demod = gst_element_factory_make("iqamdem", "demod");
	}
	g_assert(demod);
 
	aconvout = gst_element_factory_make("audioconvert", "aconvout");
	g_assert(aconvout);
	audiosink = gst_element_factory_make ("osssink", "play_audio");
	g_assert (audiosink);

	gst_bin_add_many (GST_BIN (bin), filesrc, decoder, aconvin, cmplxin,
	    tee, queue1,
	    queue2, cmplxfft, waterfall, imagesink,
	    filter, polar, demod, aconvout, audiosink,
	    NULL);

	gst_element_link(filesrc, decoder);
	g_signal_connect(decoder, "pad-added", G_CALLBACK(new_pad), aconvin);
	gst_element_link_many(aconvin, cmplxin, tee, NULL);
	gst_element_link_many(tee, queue1, filter, polar, demod,
	    aconvout, audiosink, NULL);
	gst_element_link_many(tee, queue2, cmplxfft, waterfall, imagesink, NULL);

	printf("Playing\n");
	gst_element_set_state (bin, GST_STATE_PLAYING);
	event_loop (bin);
	gst_element_set_state (bin, GST_STATE_NULL);
	printf("Done\n");

	exit (0);
}
