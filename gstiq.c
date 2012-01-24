/*
 *	Library with Quadrature related filters.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#include "gstiq.h"

static gboolean plugin_init (GstPlugin *plugin)
{
	if (!gst_element_register(plugin, "iqcmplx", GST_RANK_NONE,
	    GST_TYPE_IQCMPLX))
	    	return FALSE;
	if (!gst_element_register(plugin, "iqfshift", GST_RANK_NONE,
	    GST_TYPE_IQFSHIFT))
		return FALSE;
	if (!gst_element_register(plugin, "iqpolar", GST_RANK_NONE,
	    GST_TYPE_IQPOLAR))
		return FALSE;
	if (!gst_element_register(plugin, "iqfmdem", GST_RANK_NONE,
	    GST_TYPE_IQFMDEM))
		return FALSE;
	if (!gst_element_register(plugin, "firblock", GST_RANK_NONE,
	    GST_TYPE_FIRBLOCK))
	    	return FALSE;
	if (!gst_element_register(plugin, "cmplxfft", GST_RANK_NONE,
	    GST_TYPE_CMPLXFFT))
		return FALSE;
	if (!gst_element_register(plugin, "waterfall", GST_RANK_NONE,
	    GST_TYPE_WATERFALL))
		return FALSE;
	if (!gst_element_register(plugin, "afc", GST_RANK_NONE,
	    GST_TYPE_AFC))
		return FALSE;
	if (!gst_element_register(plugin, "iqamdem", GST_RANK_NONE,
	    GST_TYPE_IQAMDEM))
		return FALSE;
	if (!gst_element_register(plugin, "bpskrcdem", GST_RANK_NONE,
	    GST_TYPE_BPSKRCDEM))
		return FALSE;
	if (!gst_element_register(plugin, "bpskrcmod", GST_RANK_NONE,
	    GST_TYPE_BPSKRCMOD))
		return FALSE;
	if (!gst_element_register(plugin, "manchestermod", GST_RANK_NONE,
	    GST_TYPE_MANCHESTERMOD))
		return FALSE;
	if (!gst_element_register(plugin, "nrzikiss", GST_RANK_NONE,
	    GST_TYPE_NRZIKISS))
		return FALSE;
	if (!gst_element_register(plugin, "kissnrzi", GST_RANK_NONE,
	    GST_TYPE_KISSNRZI))
		return FALSE;
	
	return TRUE;
}

GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR, GST_VERSION_MINOR,
	"iq",
	"Quadrature software radio and related filters",
	plugin_init,
	IQ_VERSION,
	"GPL", 		/* This is NOT a mistake... */
	"libgstiq",
	"http://sharon.esrac.ele.tue.nl/users/pe1rxq/libgstiq/"
);
