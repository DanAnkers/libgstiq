#
#	Makefile for libgstiq test programs.
#

CFLAGS= -Wall `pkg-config gstreamer-0.10 --cflags`
LDFLAGS= `pkg-config gstreamer-0.10 --libs`

all: softrx satrx kiss2asc

softrx: softrx.o
	$(CC) $(CFLAGS) softrx.o -o softrx $(LDFLAGS)

satrx: satrx.o
	$(CC) $(CFLAGS) satrx.o -o satrx $(LDFLAGS)

kiss2asc: kiss2asc.o
	$(CC) $(CFLAGS) kiss2asc.o -o kiss2asc

clean:
	rm -rf *.o softrx satrx

