#
#	Makefile for Gstreamer Quadrature library.
#

CFLAGS= -Wall `pkg-config gstreamer-0.10 --cflags`
LDFLAGS= `pkg-config gstreamer-0.10 --libs` -lfftw3f
INSTALL= cp -p -f

GSTIQOBJS= gstiq.o \
	   cmplx.o \
	   fshift.o polar.o vector.o firblock.o polarhp.o \
	   cmplxfft.o cmplxrfft.o fdemod.o waterfall.o afc.o \
	   fmdem.o amdem.o \
	   bpskrcdem.o bpskrcmod.o \
	   manchestermod.o \
	   nrzikiss.o kissnrzi.o kissstreamer.o \
	   vectorscope.o \
	   dvbmux.o

all:	gstiq

gstiq: $(GSTIQOBJS)
	$(CC) -shared -Wl,-soname,libgstiq.so -o libgstiq.so \
	   $(GSTIQOBJS) $(LDFLAGS)

install:
	@test -d /usr/lib/gstreamer-0.10 \
	&& $(INSTALL) libgstiq.so /usr/lib/gstreamer-0.10 \
	|| \
	test -d /usr/local/lib/gstreamer-0.10 \
	&& $(INSTALL) libgstiq.so /usr/local/lib/gstreamer-0.10
	gst-inspect | grep iq

clean:
	rm -rf *.o *.so
