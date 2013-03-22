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
#define MAXWIDTH		2
#define MAXHEIGHT		3
#define PDFCOLS			(1 << 11)
#define PDFROWS			(1 << 12)

static struct doc *doc;
static fbval_t pbuf[PDFROWS * PDFCOLS];	/* current page */
static int prows, pcols;		/* the dimensions of current page */

static int num = 1;
static struct termios termios;
static char filename[256];
static int mark[128];		/* mark page number */
static int mark_head[128];	/* mark head position */
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
	zoom = z;
	showpage(num, MIN(PDFROWS - fb_rows(), head * zoom / _zoom));
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
		filename, num, doc_pages(doc), zoom * 10);
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

static void reload(void)
{
	doc_close(doc);
	doc = doc_open(filename);
	showpage(num, head);
}

static int rightmost(void)
{
	int ret = 0;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = PDFCOLS - 1;
		while (j > ret && pbuf[i * PDFCOLS + j] == FB_VAL(0, 0, 0))
			j--;
		while (j > ret && pbuf[i * PDFCOLS + j] == FB_VAL(255, 255, 255))
			j--;
		if (ret < j)
			ret = j;
	}
	return ret;
}

static int leftmost(void)
{
	int ret = PDFCOLS;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = 0;
		while (j < ret && pbuf[i * PDFCOLS + j] == FB_VAL(0, 0, 0))
			j++;
		while (j < ret && pbuf[i * PDFCOLS + j] == FB_VAL(255, 255, 255))
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
	int c, c2;
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
			showpage(getcount(doc_pages(doc)), 0);
			break;
		case 'z':
			zoom_page(getcount(15));
			break;
		case 'w':
			zoom_page(zoom * fb_cols() / pcols);
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
			c2 = readkey();
			if (isalpha(c2)) {
				mark[c2] = num;
				mark_head[c2] = head / zoom;
			}
			break;
		case 'e':
			reload();
			break;
		case '`':
		case '\'':
			c2 = readkey();
			if (isalpha(c2) && mark[c2])
				showpage(mark[c2], c == '`' ? mark_head[c2] * zoom : 0);
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
			head = prows / 2;
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
			left = leftmost() - hstep / 2;
			break;
		case ']':
			left = rightmost() + hstep / 2 - fb_cols();
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
	char *hide = "\x1b[?25l";
	char *show = "\x1b[?25h";
	char *clear = "\x1b[2J";
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
	while (i + 2 < argc && argv[i][0] == '-') {
		if (argv[i][1] == 'r')
			rotate = atoi(argv[i + 1]);
		if (argv[i][1] == 'z')
			zoom = atoi(argv[i + 1]);
		if (argv[i][1] == 'p')
			num = atoi(argv[i + 1]);
		i += 2;
	}

	write(STDIN_FILENO, hide, strlen(hide));
	write(STDOUT_FILENO, clear, strlen(clear));
	printinfo();
	if (fb_init())
		return 1;
	left = (PDFCOLS - fb_cols()) / 2;
	if (FBM_BPP(fb_mode()) != sizeof(fbval_t))
		fprintf(stderr, "fbpdf: fbval_t doesn't match fb depth\n");
	else
		mainloop();
	fb_free();
	write(STDIN_FILENO, show, strlen(show));
	printf("\n");
	doc_close(doc);
	return 0;
}
