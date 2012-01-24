/*
 *	Header for library with Quadrature related filters.
 *
 *	Copyright Jeroen Vreeken (pe1rxq@amsat.org), 2005
 *
 *	This software is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 */

#ifndef _INCLUDE_GSTIQ_H_
#define _INCLUDE_GSTIQ_H_

#include <math.h>
#include <fftw3.h>
#include <gst/gst.h>

#define IQ_VERSION "0.1"
#define PACKAGE "libgstiq"

/********************************************************************
 *	Frequency shifter declarations
 */

typedef struct _Gst_iqfshift Gst_iqfshift;

struct _Gst_iqfshift {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int rate;
	float shift;
	float step;
	float angle;
};

typedef struct _Gst_iqfshift_class Gst_iqfshift_class;

struct _Gst_iqfshift_class {
	GstElementClass parent_class;
};

#define GST_TYPE_IQFSHIFT (gst_iqfshift_get_type())
#define GST_IQFSHIFT(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_IQFSHIFT, Gst_iqfshift)
#define GST_IQFSHIFT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_IQFSHIFT, Gst_iqfshift)
#define GST_IS_IQFSHIFT(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_IQFSHIFT)
#define GST_IS_IQFSHIFT_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_IQFSHIFT)

GType gst_iqfshift_get_type(void);


/********************************************************************
 *	vector to polar converter declarations
 */

typedef struct _Gst_iqpolar Gst_iqpolar;

struct _Gst_iqpolar {
	GstElement element;

	GstPad *sinkpad, *srcpad;
};

typedef struct _Gst_iqpolar_class Gst_iqpolar_class;

struct _Gst_iqpolar_class {
	GstElementClass parent_class;
};

#define GST_TYPE_IQPOLAR (gst_iqpolar_get_type())
#define GST_IQPOLAR(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_IQPOLAR, Gst_iqpolar)
#define GST_IQPOLAR_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_IQPOLAR, Gst_iqpolar)
#define GST_IS_IQPOLAR(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_IQPOLAR)
#define GST_IS_IQPOLAR_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_IQPOLAR)

GType gst_iqpolar_get_type(void);


/********************************************************************
 *	audio to and from complex converter
 */

typedef struct _Gst_iqcmplx Gst_iqcmplx;

struct _Gst_iqcmplx {
	GstElement element;

	GstPad *sinkpad, *srcpad;
};

typedef struct _Gst_iqcmplx_class Gst_iqcmplx_class;

struct _Gst_iqcmplx_class {
	GstElementClass parent_class;
};

#define GST_TYPE_IQCMPLX (gst_iqcmplx_get_type())
#define GST_IQCMPLX(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_IQCMPLX, Gst_iqcmplx)
#define GST_IQCMPLX_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_IQCMPLX, Gst_iqcmplx)
#define GST_IS_IQCMPLX(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_IQCMPLX)
#define GST_IS_IQCMPLX_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_IQCMPLX)

GType gst_iqcmplx_get_type(void);


/********************************************************************
 *	FIR block filter
 */

typedef struct _Gst_firblock Gst_firblock;

struct firblockfilter {
	gfloat *buffer;
	gfloat sum;
	gint index;
	gint size;
};

struct _Gst_firblock {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int rate;
	int channels;
	int frequency;
	int size;
	struct firblockfilter *filters;
	int depth;
	int nr;
};

typedef struct _Gst_firblock_class Gst_firblock_class;

struct _Gst_firblock_class {
	GstElementClass parent_class;
};

#define GST_TYPE_FIRBLOCK (gst_firblock_get_type())
#define GST_FIRBLOCK(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_FIRBLOCK, Gst_firblock)
#define GST_FIRBLOCK_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_FIRBLOCK, Gst_firblock)
#define GST_IS_FIRBLOCK(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_FIRBLOCK)
#define GST_IS_FIRBLOCK_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_FIRBLOCK)

GType gst_firblock_get_type(void);


/********************************************************************
 *	Complex FFT
 */

typedef struct _Gst_cmplxfft Gst_cmplxfft;

struct _Gst_cmplxfft {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	fftwf_complex *buffer;
	fftwf_plan plan;
	int length;
	int fill;

	long offset;
};

typedef struct _Gst_cmplxfft_class Gst_cmplxfft_class;

struct _Gst_cmplxfft_class {
	GstElementClass parent_class;
};

#define GST_TYPE_CMPLXFFT (gst_cmplxfft_get_type())
#define GST_CMPLXFFT(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_CMPLXFFT, Gst_cmplxfft)
#define GST_CMPLXFFT_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_CMPLXFFT, Gst_cmplxfft)
#define GST_IS_CMPLXFFT(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_CMPLXFFT)
#define GST_IS_CMPLXFFT_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_CMPLXFFT)

GType gst_cmplxfft_get_type(void);


/********************************************************************
 *	FFT Waterall
 */

typedef struct _Gst_waterfall Gst_waterfall;

struct _Gst_waterfall {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	unsigned char *buffer;
	int length;
	int height;
	int uoff;
	int voff;
	int size;
	int frame;
	int rate;
	int skip;
	float markerf;
	int marker;
	int fcnt;

	long offset;
};

typedef struct _Gst_waterfall_class Gst_waterfall_class;

struct _Gst_waterfall_class {
	GstElementClass parent_class;
};

#define GST_TYPE_WATERFALL (gst_waterfall_get_type())
#define GST_WATERFALL(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_WATERFALL, Gst_waterfall)
#define GST_WATERFALL_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_WATERFALL, Gst_waterfalll)
#define GST_IS_WATERFALL(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_WATERFALL)
#define GST_IS_WATERFALL_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_WATERFAL)

GType gst_waterfall_get_type(void);


/********************************************************************
 *	AFC
 */

typedef struct _Gst_afc Gst_afc;

struct _Gst_afc {
	GstElement element;

	GstPad *sinkpad;
	GstPad *srcpad;

	int length;
	int rate;
	int mirror;
	float afc;

	long offset;
};

typedef struct _Gst_afc_class Gst_afc_class;

struct _Gst_afc_class {
	GstElementClass parent_class;
};

#define GST_TYPE_AFC (gst_afc_get_type())
#define GST_AFC(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_AFC, Gst_afc)
#define GST_AFC_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_AFC, Gst_afc)
#define GST_IS_AFC(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_AFC)
#define GST_IS_AFC_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_AFC)

GType gst_afc_get_type(void);


/********************************************************************
 *	Frequency demodulator declarations
 */

typedef struct _Gst_iqfmdem Gst_iqfmdem;

struct _Gst_iqfmdem {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int rate;
	float prevangle;
	float deviation;
	float normal;
	float avrg;
	float avrglen;
	float filter;

	long offset;
};

typedef struct _Gst_iqfmdem_class Gst_iqfmdem_class;

struct _Gst_iqfmdem_class {
	GstElementClass parent_class;
};

#define GST_TYPE_IQFMDEM (gst_iqfmdem_get_type())
#define GST_IQFMDEM(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_IQFMDEM, Gst_iqfmdem)
#define GST_IQFMDEM_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_IQFMDEM, Gst_iqfmdem)
#define GST_IS_IQFMDEM(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_IQFMDEM)
#define GST_IS_IQFMDEM_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_IQFMDEM)

GType gst_iqfmdem_get_type(void);


/********************************************************************
 *	Amplitude demodulator declarations
 */

typedef struct _Gst_iqamdem Gst_iqamdem;

struct _Gst_iqamdem {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int rate;
	float avrg;
	float avrglen;
	float depth;
	long offset;
};

typedef struct _Gst_iqamdem_class Gst_iqamdem_class;

struct _Gst_iqamdem_class {
	GstElementClass parent_class;
};

#define GST_TYPE_IQAMDEM (gst_iqamdem_get_type())
#define GST_IQAMDEM(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_IQAMDEM, Gst_iqamdem)
#define GST_IQAMDEM_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_IQAMDEM, Gst_iqamdem)
#define GST_IS_IQAMDEM(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_IQAMDEM)
#define GST_IS_IQAMDEM_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_IQAMDEM)

GType gst_iqamdem_get_type(void);


/********************************************************************
 *	BPSK-RC demodulator declarations
 */

typedef struct _Gst_bpskrcdem Gst_bpskrcdem;

struct _Gst_bpskrcdem {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int symbolrate;	/* Outgoing symbol rate */
	int rate;	/* Incomming sample rate */
	float symbollen;

	float avrg;
	float prevangle;
	float bit;
	int count;
	int state;

	long offset;
};

typedef struct _Gst_bpskrcdem_class Gst_bpskrcdem_class;

struct _Gst_bpskrcdem_class {
	GstElementClass parent_class;
};

#define GST_TYPE_BPSKRCDEM (gst_bpskrcdem_get_type())
#define GST_BPSKRCDEM(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_BPSKRCDEM, Gst_bpskrcdem)
#define GST_BPSKRCDEM_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_BPSKRCDEM, Gst_bpskrcdem)
#define GST_IS_BPSKRCDEM(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_BPSKRCDEM)
#define GST_IS_BPSKRCDEM_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_BPSKRCDEM)

GType gst_bpskrcdem_get_type(void);


/********************************************************************
 *	BPSK-RC modulator declarations
 */

typedef struct _Gst_bpskrcmod Gst_bpskrcmod;

struct _Gst_bpskrcmod {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int symbolrate;	/* Outgoing symbol rate */
	int rate;	/* Outgoing sample rate */
	int symbollen;
	float lastsymbol;
};

typedef struct _Gst_bpskrcmod_class Gst_bpskrcmod_class;

struct _Gst_bpskrcmod_class {
	GstElementClass parent_class;
};

#define GST_TYPE_BPSKRCMOD (gst_bpskrcmod_get_type())
#define GST_BPSKRCMOD(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_BPSKRCMOD, Gst_bpskrcmod)
#define GST_BPSKRCMOD_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_BPSKRCMOD, Gst_bpskrcmod)
#define GST_IS_BPSKRCMOD(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_BPSKRCMOD)
#define GST_IS_BPSKRCMOD_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_BPSKRCMOD)

GType gst_bpskrcmod_get_type(void);


/********************************************************************
 *	Manchester modulator declarations
 */

typedef struct _Gst_manchestermod Gst_manchestermod;

struct _Gst_manchestermod {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int symbolrate;	/* Outgoing symbol rate */
	int rate;	/* Outgoing sample rate */
	int symbollen;
};

typedef struct _Gst_manchestermod_class Gst_manchestermod_class;

struct _Gst_manchestermod_class {
	GstElementClass parent_class;
};

#define GST_TYPE_MANCHESTERMOD (gst_manchestermod_get_type())
#define GST_MANCHESTERMOD(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_MANCHESTERMOD, Gst_manchestermod)
#define GST_MANCHESTERMOD_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_MANCHESTERMOD, Gst_manchestermod)
#define GST_IS_MANCHESTERMOD(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_MANCHESTERMOD)
#define GST_IS_MANCHESTERMOD_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_MANCHESTERMOD)

GType gst_manchestermod_get_type(void);


/********************************************************************
 *	NRZI KISS TNC declarations
 */

typedef struct _Gst_nrzikiss Gst_nrzikiss;

struct _Gst_nrzikiss {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	GstBuffer *buf;
	unsigned char lastbit;
	unsigned char byte;
	int bytelen;
	int ones;
	int stuffed;
	unsigned short crc;
	
	long offset;
};

typedef struct _Gst_nrzikiss_class Gst_nrzikiss_class;

struct _Gst_nrzikiss_class {
	GstElementClass parent_class;
};

#define GST_TYPE_NRZIKISS (gst_nrzikiss_get_type())
#define GST_NRZIKISS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_NRZIKISS, Gst_nrzikiss)
#define GST_NRZIKISS_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_NRZIKISS, Gst_nrzikiss)
#define GST_IS_NRZIKISS(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_NRZIKISS)
#define GST_IS_NRZIKISS_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_NRZIKISS)

GType gst_nrzikiss_get_type(void);


/********************************************************************
 *	NRZI KISS TNC declarations
 */

typedef struct _Gst_kissnrzi Gst_kissnrzi;

struct _Gst_kissnrzi {
	GstElement element;

	GstPad *sinkpad, *srcpad;

	int state;
	GstBuffer *inbuf;
	int inpos;
	int lastesc;
	guchar *buffer;
	int buflen;
	int bufsize;
	int headlen;
	int taillen;
	gfloat bit;
	int ones;
	unsigned short crc;
};

typedef struct _Gst_kissnrzi_class Gst_kissnrzi_class;

struct _Gst_kissnrzi_class {
	GstElementClass parent_class;
};

#define GST_TYPE_KISSNRZI (gst_kissnrzi_get_type())
#define GST_KISSNRZI(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, GST_TYPE_KISSNRZI, Gst_kissnrzi)
#define GST_KISSNRZI_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, GST_TYPE_KISSNRZI, Gst_kissnrzi)
#define GST_IS_KISSNRZI(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, GST_TYPE_KISSNRZI)
#define GST_IS_KISSNRZI_CLASS(obj) G_TYPE_CHECK_CLASS_TYPE(klass, GST_TYPE_KISSNRZI)

GType gst_kissnrzi_get_type(void);


#endif /* _INCLUDE_GSTIQ_H_ */
