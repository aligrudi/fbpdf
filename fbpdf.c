/*
 * fbpdf - a small framebuffer pdf viewer using mupdf
 *
 * Copyright (C) 2009-2015 Ali Gholami Rudi <ali at rudi dot ir>
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

#define PAGESTEPS	8
#define MAXZOOM		100
#define MARGIN		1
#define CTRLKEY(x)	((x) - 96)
#define ISMARK(x)	(isalpha(x) || (x) == '\'' || (x) == '`')

static struct doc *doc;
static fbval_t *pbuf;		/* current page */
static int srows, scols;	/* screen dimentions */
static int prows, pcols;	/* current page dimensions */
static int prow, pcol;		/* page position */
static int srow, scol;		/* screen position */

static struct termios termios;
static char filename[256];
static int mark[128];		/* mark page number */
static int mark_row[128];	/* mark head position */
static int num = 1;		/* page number */
static int numdiff;		/* G command page number difference */
static int zoom = 15;
static int zoom_def = 15;	/* default zoom */
static int rotate;
static int count;

static void draw(void)
{
	int i;
	fbval_t *rbuf = malloc(scols * sizeof(rbuf[0]));
	for (i = srow; i < srow + srows; i++) {
		int cbeg = MAX(scol, pcol);
		int cend = MIN(scol + scols, pcol + pcols);
		memset(rbuf, 0, scols * sizeof(rbuf[0]));
		if (i >= prow && i < prow + prows && cbeg < cend) {
			memcpy(rbuf + cbeg - scol,
				pbuf + (i - prow) * pcols + cbeg - pcol,
				(cend - cbeg) * sizeof(rbuf[0]));
		}
		fb_set(i - srow, 0, rbuf, scols);
	}
	free(rbuf);
}

static int loadpage(int p)
{
	if (p < 1 || p > doc_pages(doc))
		return 1;
	prows = 0;
	free(pbuf);
	pbuf = doc_draw(doc, p, zoom, rotate, &prows, &pcols);
	prow = -prows / 2;
	pcol = -pcols / 2;
	num = p;
	return 0;
}

static void zoom_page(int z)
{
	int _zoom = zoom;
	zoom = MIN(MAXZOOM, MAX(1, z));
	if (!loadpage(num))
		srow = srow * zoom / _zoom;
}

static void setmark(int c)
{
	if (ISMARK(c)) {
		mark[c] = num;
		mark_row[c] = srow / zoom;
	}
}

static void jmpmark(int c, int offset)
{
	if (c == '`')
		c = '\'';
	if (ISMARK(c) && mark[c]) {
		int dst = mark[c];
		int dst_row = offset ? mark_row[c] * zoom : 0;
		setmark('\'');
		if (!loadpage(dst))
			srow = offset ? dst_row : prow;
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
	printf("FBPDF:     file:%s  page:%d(%d)  zoom:%d%% \x1b[K\r",
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
	printf("\x1b[?25l");		/* hide the cursor */
	printf("\x1b[2J");		/* clear the screen */
	fflush(stdout);
}

static void term_cleanup(void)
{
	tcsetattr(0, 0, &termios);
	printf("\x1b[?25h\n");		/* show the cursor */
}

static void sigcont(int sig)
{
	term_setup();
}

static int reload(void)
{
	doc_close(doc);
	doc = doc_open(filename);
	if (!doc || !doc_pages(doc)) {
		fprintf(stderr, "\nfbpdf: cannot open <%s>\n", filename);
		return 1;
	}
	if (!loadpage(num))
		draw();
	return 0;
}

static int rmargin(void)
{
	int ret = 0;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = pcols - 1;
		while (j > ret && pbuf[i * pcols + j] == FB_VAL(255, 255, 255))
			j--;
		if (ret < j)
			ret = j;
	}
	return ret;
}

static int lmargin(void)
{
	int ret = pcols;
	int i, j;
	for (i = 0; i < prows; i++) {
		j = 0;
		while (j < ret && pbuf[i * pcols + j] == FB_VAL(255, 255, 255))
			j++;
		if (ret > j)
			ret = j;
	}
	return ret;
}

static void mainloop(void)
{
	int step = srows / PAGESTEPS;
	int hstep = scols / PAGESTEPS;
	int c;
	term_setup();
	signal(SIGCONT, sigcont);
	loadpage(num);
	srow = prow;
	scol = -scols / 2;
	draw();
	while ((c = readkey()) != -1) {
		if (c == 'q')
			break;
		if (c == 'e' && reload())
			break;
		switch (c) {	/* commands that do not require redrawing */
		case 'o':
			numdiff = num - getcount(num);
			break;
		case 'Z':
			zoom_def = getcount(zoom);
			break;
		case 'i':
			printinfo();
			break;
		case 27:
			count = 0;
			break;
		case 'm':
			setmark(readkey());
			break;
		case 'd':
			sleep(getcount(1));
			break;
		default:
			if (isdigit(c))
				count = count * 10 + c - '0';
		}
		switch (c) {	/* commands that require redrawing */
		case CTRLKEY('f'):
		case 'J':
			if (!loadpage(num + getcount(1)))
				srow = prow;
			break;
		case CTRLKEY('b'):
		case 'K':
			if (!loadpage(num - getcount(1)))
				srow = prow;
			break;
		case 'G':
			setmark('\'');
			if (!loadpage(getcount(doc_pages(doc) - numdiff) + numdiff))
				srow = prow;
			break;
		case 'O':
			numdiff = num - getcount(num);
			setmark('\'');
			if (!loadpage(num + numdiff))
				srow = prow;
			break;
		case 'z':
			zoom_page(getcount(zoom_def));
			break;
		case 'w':
			zoom_page(pcols ? zoom * scols / pcols : zoom);
			break;
		case 'W':
			if (lmargin() < rmargin())
				zoom_page(zoom * (scols - hstep) /
					(rmargin() - lmargin()));
			break;
		case 'f':
			zoom_page(prows ? zoom * srows / prows : zoom);
			break;
		case 'r':
			rotate = getcount(0);
			if (!loadpage(num))
				srow = prow;
			break;
		case '`':
		case '\'':
			jmpmark(readkey(), c == '`');
			break;
		case 'j':
			srow += step * getcount(1);
			break;
		case 'k':
			srow -= step * getcount(1);
			break;
		case 'l':
			scol += hstep * getcount(1);
			break;
		case 'h':
			scol -= hstep * getcount(1);
			break;
		case 'H':
			srow = prow;
			break;
		case 'L':
			srow = prow + prows - srows;
			break;
		case 'M':
			srow = prow + prows / 2 - srows / 2;
			break;
		case 'C':
			scol = -scols / 2;
			break;
		case ' ':
		case CTRLKEY('d'):
			srow += srows * getcount(1) - step;
			break;
		case 127:
		case CTRLKEY('u'):
			srow -= srows * getcount(1) - step;
			break;
		case '[':
			scol = pcol;
			break;
		case ']':
			scol = pcol + pcols - scols;
			break;
		case '{':
			scol = pcol + lmargin() - hstep / 2;
			break;
		case '}':
			scol = pcol + rmargin() + hstep / 2 - scols;
			break;
		case CTRLKEY('l'):
			break;
		default:	/* no need to redraw */
			continue;
		}
		srow = MAX(prow - srows + MARGIN, MIN(prow + prows - MARGIN, srow));
		scol = MAX(pcol - scols + MARGIN, MIN(pcol + pcols - MARGIN, scol));
		draw();
	}
	term_cleanup();
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
	if (!doc || !doc_pages(doc)) {
		fprintf(stderr, "fbpdf: cannot open <%s>\n", filename);
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
	printinfo();
	if (fb_init())
		return 1;
	srows = fb_rows();
	scols = fb_cols();
	if (FBM_BPP(fb_mode()) != sizeof(fbval_t))
		fprintf(stderr, "fbpdf: fbval_t doesn't match fb depth\n");
	else
		mainloop();
	fb_free();
	free(pbuf);
	if (doc)
		doc_close(doc);
	return 0;
}
