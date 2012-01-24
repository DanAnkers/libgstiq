/***************************************************************\
*                                                               *
* KISS2ASC.C  -- Convert KISS to ASCII                          *
*                                                               *
* Copyright 1990, 1991 Paul Williamson, KB5MU                   *
*                                                               *
* This program turns a KISS capture file into a readable        *
* ASCII file.                                                   *
*                                                               *
* When invoked without arguments, it produces a usage message.  *
*                                                               *
* If invoked with a single filename argument, it reads the      *
* specified KISS file and sends the ASCII output to stdout.     *
* If invoked with two filenames, it reads the first file and    *
* sends the ASCII output to the second file.                    *
*                                                               *
* Compiled with Microsoft C 6.00, using command line:           *
*        cl -Gr -Ox -W4 kiss2asc.c                              *
* Or with Turbo C 2.00, using command line:                     *
*        tcc -w -G -O -Z -p kiss2asc.c                          *
*                                                               *
\***************************************************************/
/*
 *	Changes:
 *
 *	PE1RXQ: disable buffering for realtime decoding.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define	BUFLEN 500

		/* Special characters for KISS */

#define	FEND		0300	/* Frame End */
#define	FESC		0333	/* Frame Escape */
#define	TFEND		0334	/* Transposed frame end */
#define	TFESC		0335	/* Transposed frame escape */
 
		/* Function Prototypes */

int main(int argc, char **argv);
int get_frame(char *buf, FILE *infile);
void asc_frame(char *buf, int len, FILE *outfile);
void asc_data(char *buf, int len, FILE *outfile);
char *asc_header(char *buf, FILE *outfile);
char *getcall(char *framebuf, char *callbuf);

/*******************\
*                   *
* The Main Program  *
*                   *
* (obviously)       *
*                   *
\*******************/

int main(int argc, char **argv)
{
FILE	*infile, *outfile;
int	status = 0;
char	buffer[BUFLEN];
int	len;

				/* Check the command line arguments */
if ((argc < 1) || (argc > 3))
    {
    puts("KISS2ASC - Convert binary KISS file to readable ASCII");
    puts("    Version 1.1   8/14/91 Copyright 1990, 1991 Paul Williamson KB5MU");
    puts("Usage:");
    puts("    kiss2asc infile [outfile]");
    exit(1);
    }

if (argc==1) {
	infile = stdin;
} else						/* Open the input file */
if ((infile = fopen(argv[1], "rb")) == NULL)
    {
    printf("Can't open file %s.\n", argv[1]);
    exit(1);
    }
			/* Open the output file, if specified */
if (argc == 3)
    {
    if ((outfile = fopen(argv[2], "w")) == NULL)
	{
	printf("Can't open file %s.\n", argv[2]);
	fclose(infile);
	exit(1);
	}
    }
else			/* or use stdout if not specified */
    outfile = stdout;

			/* Try to use big buffers to speed things up */
//if (setvbuf(infile, (char *)NULL, _IOFBF, 1) ||
//    setvbuf(outfile,(char *)NULL, _IOFBF, 1))
//    puts("Couldn't use full buffering, so this may be slow.");

			/* Here's the main loop.  Get a frame, convert
			   it to ASCII, and output it; until we run out
			   of frames to convert.  */

while ( ((len=get_frame(buffer, infile))>=0) &&
	!ferror(infile) &&
	!ferror(outfile))
    asc_frame(buffer, len, outfile);

if (len == -1)
    fprintf(outfile, "*** Ill-formed frame - aborting ***\n");

			/* wrap up the input file */
if (ferror(infile))
    {
    puts("Error reading input file.");
    status = 1;
    }

			/* wrap up the output file */
if (ferror(outfile))
    {
    puts("Error writing output file.");
    status = 1;
    }

if (fclose(outfile))
    {
    puts("Error closing output file.");
    status = 1;
    }

return status;		/* return value for batch files */
}

/***************************************************************\
*                                                               *
* get_frame                                                     *
*                                                               *
* This function gets one complete KISS frame of data from       *
* the specified input file, and places it in buf.  It returns   *
* the number of bytes placed in buf, including possibly 0 if    *
* there is an empty KISS frame (C0 00 C0) in the file.  If an   *
* ill-formed frame is found, it returns -1.  If no more data    *
* is available due to EOF, it returns -2.                       *
*                                                               *
* See the ARRL 6th Computer Networking Conference Proceedings,  *
* "The KISS TNC: A simple Host-to-TNC communications protocol"  *
* by Mike Chepponis, K3MC, and Phil Karn, KA9Q, for details of  *
* the KISS framing standard.                                    *
*                                                               *
\***************************************************************/

int get_frame(char *buf, FILE *infile)
{
char	*bufp = buf;
int	chr;

while (((chr = getc(infile)) != FEND) && !feof(infile))
    ;				/* eat bytes up to first FEND */

while (((chr = getc(infile)) == FEND) && !feof(infile))
    ;				/* eat as many FENDs as we find */

chr = getc(infile);		/* get past the nul frame-type byte */

while ((chr != FEND) && !feof(infile))
    {
    if (chr == FESC)
	{
	chr = getc(infile);
	if (chr == TFESC)
	    *bufp++ = FESC;		/* put escaped character in buffer */
	else if (chr == TFEND)
	    *bufp++ = FEND;
        else
	    return -1;			/* ill-formed frame */
	}
    else
	*bufp++ = (char)chr;		/* put character in buffer */

    chr = getc(infile);
    }

if (feof(infile))			/* Special return for EOF */
    return -2;
else
    return (bufp - buf);		/* return length of buffer */
}

/***************************************************************\
*                                                               *
* asc_frame                                                     *
*                                                               *
* This function handles a single frame.  It calls functions to  *
* translate the AX.25 header and the data field to ASCII.       *
*                                                               *
* If the frame is empty or too short, it handles that too.      *
*                                                               *
\***************************************************************/

void asc_frame(char *buf, int len, FILE *outfile)
{
char	*p;
int	i;

if (len == 0)			/* Special handling for empty frames */
    fprintf(outfile, "*** Empty Frame ***\n");

else if (len < 15)		/* Special handling for short frames */
    {
    fprintf(outfile, "*** Short Frame: ");
    for (i=0; i<len; i++)
	fprintf(outfile, "%02X ", *buf++ & 0xFF);
    fprintf(outfile, "\n");
    }
			/* translate the AX.25 header to ASCII */
else if ((p=asc_header(buf, outfile)) == NULL)
    return;			/* no data field to log */

else			/* if there's data, output it too */
    asc_data(p, len - (p-buf), outfile);
}

/*********************************************************\
*                                                         *
* asc_header                                              *
*                                                         *
* This function decodes the header of a frame.            *
*                                                         *
* It outputs an ASCII translation of the header to the    *
* specified output file.  If the frame has an associated  *
* information field, it returns a pointer to the first    *
* byte of the information field.  If not, it returns      *
* the NULL pointer.                                       *
*                                                         *
* The ASCII translation generated resembles the monitor   *
* headers generated by the WA8DED TNC firmware.           *
*                                                         *
\*********************************************************/

/* pollcode gives the flags used to represent the various sets of
   poll and final flags.  It is indexed by three bits:
	pollcode[0x10 bit of ctl][0x80 bit of destcall][0x80 bit of fromcall]

   We use the following codes, following WA8DED:
		!	version 1 with poll/final
		^	version 2 command without poll
		+	version 2 command with poll
		-	version 2 response with final
		v	version 2 response without final

0 in the table indicates no character is to be output.
*/
		

char	pollcode[2][2][2] = {	'0',	/* 0 0 0 = V1, no poll/final */ 
				'v',	/* 0 0 1 = V2 resp, no final */
				'^',	/* 0 1 0 = V2 cmd, no poll */
				0,	/* 0 1 1 = V1, no poll/final */
				'!',	/* 1 0 0 = V1, with poll/final */
				'-',	/* 1 0 1 = V2 resp, with final */
				'+',	/* 1 1 0 = V2 cmd, with poll */
				'!',	/* 1 1 1 = V1, with poll/final */
			    };

char	*frameid[] = {"RR", "RNR", "REJ", "???",	/* S frames */
		"SABM", "DISC", "DM", "UA", "FRMR"}; 	/* U frames */

char *asc_header(char *buf, FILE *outfile)
{
char	*bufp;
char	call[10], call2[10];
char	ctl;
int	id = 3;
int	pollchr;

			/* output the source and destination calls */
fprintf(outfile, "fm %s to %s ", getcall(buf+7, call), getcall(buf, call2));

bufp = buf+14;			/* point to CTL or first digipeater */

			/* output the digipeater calls, if any */
if ((buf[13] & 1) == 0)		/* if there are digipeaters, */
    {
    fprintf(outfile, "via ");
    for (bufp = buf+14; !(*(bufp-1) & 1) && bufp < buf+70; bufp+=7)
	fprintf(outfile, (bufp[6] & 0x80) ? "%s* " : "%s ",
			getcall(bufp, call));
    }

ctl = *bufp++;			/* translate the ctl byte */
pollchr = pollcode[!!(ctl&0x10)][!!(buf[6]&0x80)][!!(buf[13]&0x80)];

	/* now handle each frame type separately */

if ((ctl & 1) == 0)		/* if it's an I frame */
    {
    fprintf(outfile, "ctl I%d%d", ((ctl & 0xE0) >> 5), ((ctl & 0x0E) >> 1));
    if (pollchr)
	fprintf(outfile, "%c", pollchr);	/* output poll/final mark */
    fprintf(outfile, " pid %02X\n", *bufp++ & 0xFF);	/* add PID */
    return bufp;			/* return pointer to the data */
    }
else if ((ctl & 3) == 1)	/* if it's an S frame */
    {
    fprintf(outfile, "ctl %s%d", frameid[(ctl & 0x0C) >> 2],
				((ctl & 0xE0) >> 5));
    if (pollchr)
	fprintf(outfile, "%c", pollchr);	/* output poll/final mark */
    putc('\n', outfile);
    return NULL;			/* no data */
    }
else if ((ctl & 0xEF) == 0x03)	/* if it's a UI frame */
    {
    fprintf(outfile, "ctl UI");
#ifdef WHYDOTHIS
    if (pollchr)
	fprintf(outfile, "%c", pollchr);	/* output poll/final mark */
#endif
    fprintf(outfile, " pid %02X\n", *bufp++ & 0xFF);	/* add PID */
    return bufp;			/* return pointer to the data */
    }
else				/* it's some other U frame */
    {
    switch (ctl & 0xEF)
	{
	case 0x2F:		/* SABM frame */
		id = 4;
		break;
	case 0x43:		/* DISC frame */
		id = 5;
		break;
	case 0x0F:		/* DM frame */
		id = 6;
		break;
	case 0x63:		/* UA frame */
		id = 7;
		break;
	case 0x87:		/* FRMR frame */
		id = 8;
		break;
	}
    fprintf(outfile, "ctl %s", frameid[id]);
    if (pollchr)
	fprintf(outfile, "%c", pollchr);	/* output poll/final mark */
    putc('\n', outfile);
    return NULL;			/* no data */
    }
}

/**************************************************************\
*                                                              *
* getcall                                                      *
*                                                              *
* This function translates a callsign from the AX.25 format    *
* in the frame buffer to ASCII format in the callsign buffer.  *
*                                                              *
* Returns the callsign buffer, for convenient use.             *
*                                                              *
\**************************************************************/

char *getcall(char *framebuf, char *callbuf)
{
int	i;
int	ssid;
char	*cbp = callbuf;

for (i=0; i<6; i++)		/* start by filling in the basic characters */
    if (*framebuf == 0x40)
	framebuf++;
    else
	*cbp++ = (char)((*framebuf++ >> 1) & 0x7F);	/* unshift to ASCII */

				/* tack on the SSID if it isn't zero */
if ((ssid = (*framebuf & 0x1E) >> 1) > 0)
    sprintf(cbp, "-%d", ssid);
else
    *cbp = '\0';

return callbuf;
}

/*************************************************************\
*                                                             *
* asc_data                                                    *
*                                                             *
* This function takes the data field of a frame, and logs it  *
* to the specified file.  It filters all binary characters    *
* to dots.  It ends the field with an extra newline.          *
*                                                             *
\*************************************************************/

void asc_data(char *buf, int len, FILE *outfile)
{
char	*lastp, *p;

lastp = buf+len;
for (p = buf; p < lastp; p++)		/* for each character in the buffer */
    if ((isascii(*p) && isprint(*p)) || (*p == '\t'))
	putc(*p, outfile);
    else if (*p == '\r')		/* translate CR to CRLF */
	putc('\n', outfile);
    else
	putc('.', outfile);		/* ugly binary chars mapped to '.' */

putc('\n', outfile);		/* terminate the packet */

}
