/*
 * fbpdf - a small framebuffer pdf viewer using mupdf
 *
 * Copyright (C) 2009-2013 Ali Gholami Rudi <ali at rudi dot ir>
 *
 * This program is released under the Modified BSD license.
 */
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include "draw.h"
#include "doc.h"

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))

#define PAGESTEPS		8
#define CTRLKEY(x)		((x) - 96)
#define ISMARK(x)		(isalpha(x) || (x) == '\'' || (x) == '`')
#define MAXWIDTH		2
#define MAXHEIGHT		3
#define PDFCOLS			(1 << 11)
#define PDFROWS			(1 << 12)
#define MAXZOOM			(100)

static struct doc *doc;
static fbval_t pbuf[PDFROWS * PDFCOLS];	/* current page */
static int prows, pcols;		/* the dimensions of current page */

static struct termios termios;
static char filename[256];
static int mark[128];		/* mark page number */
static int mark_head[128];	/* mark head position */
static int num = 1;		/* page number */
static int numdiff;		/* G command page number difference */
static int zoom = 15;
static int rotate;
static int head;
static int left;
static int count;

static void draw(void)
{
	int i;
	for (i = head; i < MIN(head + fb_rows(), PDFROWS); i++)
		fb_set(i - head, 0, pbuf + i * PDFCOLS + left, fb_cols());
}

static int showpage(int p, int h)
{
	if (p < 1 || p > doc_pages(doc))
		return 0;
	memset(pbuf, 0x00, sizeof(pbuf));
	prows = PDFROWS;
	pcols = PDFCOLS;
	doc_draw(doc, p, zoom, rotate, pbuf, &prows, &pcols);
	num = p;
	head = h;
	draw();
	return 0;
}

static void zoom_page(int z)
{
	int _zoom = zoom;
	zoom = MIN(MAXZOOM, MAX(1, z));
	showpage(num, MIN(PDFROWS - fb_rows(), head * zoom / _zoom));
}

static void setmark(int c)
{
	if (ISMARK(c)) {
		mark[c] = num;
		mark_head[c] = head / zoom;
	}
}

static void jmpmark(int c, int offset)
{
	if (c == '`')
		c = '\'';
	if (ISMARK(c) && mark[c]) {
		int dst = mark[c];
		int dst_head = offset ? mark_head[c] * zoom : 0;
		setmark('\'');
		showpage(dst, dst_head);
	}
}

static int readkey(void)
{
	unsigned char b;
	if (read(0, &b, 1) <= 0)
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
		filename, num, doc_pages(doc), zoom * 10);
	fflush(stdout);
}

static void term_setup(void)
{
	struct termios newtermios;
	tcgetattr(0, &termios);
	newtermios = termios;
	newtermios.c_lflag &= ~ICANON;
	newtermios.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &newtermios);
}

static void term_cleanup(void)
{
	tcsetattr(0, 0, &termios);
}

static void sigcont(int sig)
{
	term_setup();
}

static void reload(void)
{
	doc_close(doc);
	doc = doc_open(filename);
	showpage(num, head);
}

static int rightmost(int cont)
{
	int ret = 0;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = PDFCOLS - 1;
		while (j > ret && pbuf[i * PDFCOLS + j] == FB_VAL(0, 0, 0))
			j--;
		while (cont && j > ret &&
				pbuf[i * PDFCOLS + j] == FB_VAL(255, 255, 255))
			j--;
		if (ret < j)
			ret = j;
	}
	return ret;
}

static int leftmost(int cont)
{
	int ret = PDFCOLS;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = 0;
		while (j < ret && pbuf[i * PDFCOLS + j] == FB_VAL(0, 0, 0))
			j++;
		while (cont && j < ret &&
				pbuf[i * PDFCOLS + j] == FB_VAL(255, 255, 255))
			j++;
		if (ret > j)
			ret = j;
	}
	return ret;
}

static void mainloop(void)
{
	int step = fb_rows() / PAGESTEPS;
	int hstep = fb_cols() / PAGESTEPS;
	int c;
	term_setup();
	signal(SIGCONT, sigcont);
	showpage(num, 0);
	while ((c = readkey()) != -1) {
		switch (c) {
		case CTRLKEY('f'):
		case 'J':
			showpage(num + getcount(1), 0);
			break;
		case CTRLKEY('b'):
		case 'K':
			showpage(num - getcount(1), 0);
			break;
		case 'G':
			setmark('\'');
			showpage(getcount(doc_pages(doc) - numdiff) + numdiff, 0);
			break;
		case 'z':
			zoom_page(getcount(15));
			break;
		case 'w':
			zoom_page(zoom * fb_cols() / pcols);
			break;
		case 'W':
			if (leftmost(1) < rightmost(1))
				zoom_page(zoom * (fb_cols() - hstep) /
					(rightmost(1) - leftmost(1)));
			break;
		case 'f':
			zoom_page(zoom * fb_rows() / prows);
			break;
		case 'r':
			rotate = getcount(0);
			showpage(num, 0);
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
		case 'm':
			setmark(readkey());
			break;
		case 'e':
			reload();
			break;
		case '`':
		case '\'':
			jmpmark(readkey(), c == '`');
			break;
		case 'o':
			numdiff = num - getcount(num);
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
			head = MAX(0, prows - fb_rows());
			break;
		case 'M':
			head = (prows - fb_rows()) / 2;
			break;
		case ' ':
		case CTRL('d'):
			head += fb_rows() * getcount(1) - step;
			break;
		case 127:
		case CTRL('u'):
			head -= fb_rows() * getcount(1) - step;
			break;
		case '[':
			left = leftmost(0);
			break;
		case ']':
			left = rightmost(0) - fb_cols();
			break;
		case '{':
			left = leftmost(1) - hstep / 2;
			break;
		case '}':
			left = rightmost(1) + hstep / 2 - fb_cols();
			break;
		case CTRLKEY('l'):
			break;
		default:
			/* no need to redraw */
			continue;
		}
		head = MAX(0, MIN(PDFROWS - fb_rows(), head));
		left = MAX(0, MIN(PDFCOLS - fb_cols(), left));
		draw();
	}
}

static char *usage =
	"usage: fbpdf [-r rotation] [-z zoom x10] [-p page] filename\n";

int main(int argc, char *argv[])
{
	int i = 1;
	if (argc < 2) {
		printf(usage);
		return 1;
	}
	strcpy(filename, argv[argc - 1]);
	doc = doc_open(filename);
	if (!doc) {
		fprintf(stderr, "cannot open <%s>\n", filename);
		return 1;
	}
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		switch (argv[i][1]) {
		case 'r':
			rotate = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]);
			break;
		case 'z':
			zoom = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]);
			break;
		case 'p':
			num = atoi(argv[i][2] ? argv[i] + 2 : argv[++i]);
			break;
		}
	}
	printf("\x1b[?25l");		/* hide the cursor */
	printf("\x1b[2J");		/* clear the screen */
	printinfo();
	if (fb_init())
		return 1;
	left = (PDFCOLS - fb_cols()) / 2;
	if (FBM_BPP(fb_mode()) != sizeof(fbval_t))
		fprintf(stderr, "fbpdf: fbval_t doesn't match fb depth\n");
	else
		mainloop();
	fb_free();
	printf("\x1b[?25h\n");		/* show the cursor */
	doc_close(doc);
	return 0;
}
