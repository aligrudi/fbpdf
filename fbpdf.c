/*
 * fbpdf - a small framebuffer pdf viewer using mupdf
 *
 * Copyright (C) 2009-2011 Ali Gholami Rudi
 *
 * This program is released under GNU GPL version 2.
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

static fbval_t pbuf[PDFROWS * PDFCOLS];

static int num = 1;
static struct termios termios;
static char filename[256];
static int zoom = 15;
static int rotate;
static int head;
static int left;
static int count;

static struct doc *doc;
static int pagecount;

static void draw(void)
{
	int i;
	for (i = head; i < MIN(head + fb_rows(), PDFROWS); i++)
		fb_set(i - head, 0, pbuf + i * PDFCOLS + left, fb_cols());
}

static int showpage(int p)
{
	if (p < 1 || p > pagecount)
		return 0;
	memset(pbuf, 0x00, sizeof(pbuf));
	doc_draw(doc, pbuf, p, PDFROWS, PDFCOLS, zoom, rotate);
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

static void mainloop(void)
{
	int step = fb_rows() / PAGESTEPS;
	int hstep = fb_cols() / PAGESTEPS;
	int c;
	term_setup();
	signal(SIGCONT, sigcont);
	showpage(num);
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
		case 'r':
			rotate = getcount(0);
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
		printf("cannot open file\n");
		return 1;
	}
	pagecount = doc_pages(doc);
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
