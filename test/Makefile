#
#	Makefile for libgstiq test programs.
#

CFLAGS= -Wall `pkg-config gstreamer-0.9 --cflags`
LDFLAGS= `pkg-config gstreamer-0.9 --libs`

all: softrx satrx kiss2asc

softrx: softrx.o
	$(CC) $(LDFLAGS) $(CFLAGS) softrx.o -o softrx

satrx: satrx.o
	$(CC) $(LDFLAGS) $(CFLAGS) satrx.o -o satrx

kiss2asc: kiss2asc.o
	$(CC) $(CFLAGS) kiss2asc.o -o kiss2asc

clean:
	rm -rf *.o softrx satrx

