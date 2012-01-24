#
#	Makefile for Gstreamer Quadrature library.
#

CFLAGS= -Wall `pkg-config gstreamer-0.9 --cflags`
LDFLAGS= `pkg-config gstreamer-0.9 --libs` -lfftw3f
INSTALL= cp -p -f

GSTIQOBJS= gstiq.o \
	   cmplx.o \
	   fshift.o polar.o firblock.o \
	   cmplxfft.o waterfall.o afc.o \
	   fmdem.o amdem.o \
	   bpskrcdem.o bpskrcmod.o \
	   manchestermod.o \
	   nrzikiss.o kissnrzi.o

all:	gstiq

gstiq: $(GSTIQOBJS)
	$(CC) -shared -Wl,-soname,libgstiq.so -o libgstiq.so \
	   $(GSTIQOBJS) $(LDFLAGS)

install:
	@test -d /usr/lib/gstreamer-0.9 \
	&& $(INSTALL) libgstiq.so /usr/lib/gstreamer-0.9 \
	|| \
	test -d /usr/local/lib/gstreamer-0.9 \
	&& $(INSTALL) libgstiq.so /usr/local/lib/gstreamer-0.9
	gst-register | grep iq


clean:
	rm -rf *.o *.so
