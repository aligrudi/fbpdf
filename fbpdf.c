/*
 * fbpdf - a small framebuffer pdf viewer using mupdf
 *
 * Copyright (C) 2009-2010 Ali Gholami Rudi
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, as published by the
 * Free Software Foundation.
 */
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include "fitz.h"
#include "mupdf.h"
#include "draw.h"

#define PAGESTEPS		8
#define CTRLKEY(x)		((x) - 96)
#define MAXWIDTH		2
#define MAXHEIGHT		3
#define PDFCOLS			(1 << 11)
#define PDFROWS			(1 << 12)

static fbval_t pbuf[PDFROWS * PDFCOLS];

static int num = 1;
static struct termios termios;
static char filename[256];
static int zoom = 15;
static int head;
static int left;
static int count;

static fz_glyphcache *glyphcache;
static pdf_xref *xref;
static int pagecount;

static void draw(void)
{
	int i;
	for (i = head; i < MIN(head + fb_rows(), PDFROWS); i++)
		fb_set(i - head, 0, pbuf + i * PDFCOLS + left, fb_cols());
}

static int showpage(int p)
{
	fz_matrix ctm;
	fz_bbox bbox;
	fz_pixmap *pix;
	fz_device *dev;
	fz_obj *pageobj;
	fz_displaylist *list;
	pdf_page *page;
	int x, y, w, h;
	if (p < 1 || p > pagecount)
		return 0;

	memset(pbuf, 0x00, sizeof(pbuf));

	pageobj = pdf_getpageobject(xref, p);
	if (pdf_loadpage(&page, xref, pageobj))
		return 1;
	list = fz_newdisplaylist();
	dev = fz_newlistdevice(list);
	if (pdf_runpage(xref, page, dev, fz_identity))
		return 1;
	fz_freedevice(dev);

	ctm = fz_translate(0, -page->mediabox.y1);
	ctm = fz_concat(ctm, fz_scale((float) zoom / 10, (float) -zoom / 10));
	bbox = fz_roundrect(fz_transformrect(ctm, page->mediabox));
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	pix = fz_newpixmapwithrect(fz_devicergb, bbox);
	fz_clearpixmap(pix, 0xff);

	dev = fz_newdrawdevice(glyphcache, pix);
	fz_executedisplaylist(list, dev, ctm);
	fz_freedevice(dev);

	for (y = 0; y < MIN(pix->h, PDFROWS); y++) {
		for (x = 0; x < MIN(pix->w, PDFCOLS); x++) {
			unsigned char *s = pix->samples + y * pix->w * 4 + x * 4;
			pbuf[y * PDFCOLS + x] = fb_color(s[0], s[1], s[2]);

		}
	}
	fz_droppixmap(pix);
	fz_freedisplaylist(list);
	pdf_freepage(page);
	pdf_agestore(xref->store, 3);
	num = p;
	head = 0;
	draw();
	return 0;
}

static int readkey(void)
{
	unsigned char b;
	if (read(STDIN_FILENO, &b, 1) <= 0)
		return -1;
	return b;
}

static int getcount(int def)
{
	int result = count ? count : def;
	count = 0;
	return result;
}

static void printinfo(void)
{
	printf("\x1b[H");
	printf("FBPDF:     file:%s  page:%d(%d)  zoom:%d%% \x1b[K",
		filename, num, pagecount, zoom * 10);
	fflush(stdout);
}

static void term_setup(void)
{
	struct termios newtermios;
	tcgetattr(STDIN_FILENO, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~ICANON;
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtermios);
}

static void term_cleanup(void)
{
	tcsetattr(STDIN_FILENO, 0, &termios);
}

static void sigcont(int sig)
{
	term_setup();
}

static void mainloop()
{
	int step = fb_rows() / PAGESTEPS;
	int hstep = fb_cols() / PAGESTEPS;
	int c;
	term_setup();
	signal(SIGCONT, sigcont);
	showpage(1);
	while ((c = readkey()) != -1) {
		int maxhead = PDFROWS - fb_rows();
		int maxleft = PDFCOLS - fb_cols();
		switch (c) {
		case CTRLKEY('f'):
		case 'J':
			showpage(num + getcount(1));
			break;
		case CTRLKEY('b'):
		case 'K':
			showpage(num - getcount(1));
			break;
		case 'G':
			showpage(getcount(pagecount));
			break;
		case 'z':
			zoom = getcount(15);
			showpage(num);
			break;
		case 'i':
			printinfo();
			break;
		case 'q':
			term_cleanup();
			return;
		case 27:
			count = 0;
			break;
		default:
			if (isdigit(c))
				count = count * 10 + c - '0';
		}
		switch (c) {
		case 'j':
			head += step * getcount(1);
			break;
		case 'k':
			head -= step * getcount(1);
			break;
		case 'l':
			left += hstep * getcount(1);
			break;
		case 'h':
			left -= hstep * getcount(1);
			break;
		case 'H':
			head = 0;
			break;
		case 'L':
			head = maxhead;
			break;
		case 'M':
			head = maxhead / 2;
			break;
		case ' ':
		case CTRL('d'):
			head += fb_rows() * getcount(1) - step;
			break;
		case 127:
		case CTRL('u'):
			head -= fb_rows() * getcount(1) - step;
			break;
		case CTRLKEY('l'):
			break;
		default:
			/* no need to redraw */
			continue;
		}
		head = MAX(0, MIN(maxhead, head));
		left = MAX(0, MIN(maxleft, left));
		draw();
	}
}

int main(int argc, char *argv[])
{
	char *hide = "\x1b[?25l";
	char *show = "\x1b[?25h";
	char *clear = "\x1b[2J";
	if (argc < 2) {
		printf("usage: fbpdf filename\n");
		return 1;
	}
	strcpy(filename, argv[1]);
	fz_accelerate();
	glyphcache = fz_newglyphcache();
	if (pdf_openxref(&xref, filename, NULL)) {
		printf("cannot open file\n");
		return 1;
	}
	if (pdf_loadpagetree(xref))
		return 1;
	pagecount = pdf_getpagecount(xref);

	write(STDIN_FILENO, hide, strlen(hide));
	write(STDOUT_FILENO, clear, strlen(clear));
	printinfo();
	fb_init();
	mainloop();
	fb_free();
	write(STDIN_FILENO, show, strlen(show));
	printf("\n");

	pdf_freexref(xref);
	fz_freeglyphcache(glyphcache);
	return 0;
}
