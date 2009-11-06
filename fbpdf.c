/*
 * fbpdf - A small framebuffer pdf viewer using poppler
 *
 * Copyright (C) 2009 Ali Gholami Rudi
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, as published by the
 * Free Software Foundation.
 *
 */
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include <cairo/cairo.h>
#include <glib/poppler.h>
#include "draw.h"

#define PAGESTEPS		8
#define CTRLKEY(x)		((x) - 96)
#define MAXWIDTH		2
#define MAXHEIGHT		3

static PopplerDocument *doc;
static PopplerPage *page;
static int num;
static cairo_t *cairo;
static cairo_surface_t *surface;
static struct termios termios;
static char filename[PATH_MAX];
static int zoom = 15;
static int head;
static int left;
static int count;

static void draw()
{
	unsigned char *img = cairo_image_surface_get_data(surface);
	fbval_t slice[1 << 14];
	int i, j;
	int h = MIN(fb_rows(), cairo_image_surface_get_height(surface));
	int w = MIN(fb_cols(), cairo_image_surface_get_width(surface));
	int cols = cairo_image_surface_get_width(surface);
	for (i = head; i < h + head; i++) {
		for (j = left; j < w + left; j++) {
			unsigned char *p = img + (i * cols + j) * 4;
			slice[j - left] = fb_color(*p, *(p + 1), *(p + 2));
		}
		fb_set(i - head, 0, slice, w);
	}
}

static int load_document(void)
{
	char abspath[PATH_MAX];
	char uri[PATH_MAX + 16];
	realpath(filename, abspath);
	snprintf(uri, sizeof(uri), "file://%s", abspath);
	doc = poppler_document_new_from_file(uri, NULL, NULL);
	return !doc;
}

static void cleanup_page(void)
{
	if (cairo)
		cairo_destroy(cairo);
	if (surface)
		cairo_surface_destroy(surface);
	if (page)
		g_object_unref(G_OBJECT(page));
}

static void showpage(int p)
{
	if (p < 0 || p >= poppler_document_get_n_pages(doc))
		return;
	cleanup_page();
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		fb_cols() * MAXWIDTH, fb_rows() * MAXHEIGHT);
	cairo = cairo_create(surface);
	cairo_scale(cairo, (float) zoom / 10, (float) zoom / 10);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
	cairo_paint(cairo);
	num = p;
	page = poppler_document_get_page(doc, p);
	poppler_page_render(page, cairo);
	head = 0;
	draw();
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
		filename, num + 1, poppler_document_get_n_pages(doc), zoom * 10);
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
	showpage(0);
	while ((c = readkey()) != -1) {
		int maxhead = cairo_image_surface_get_height(surface) - fb_rows();
		int maxleft = cairo_image_surface_get_width(surface) - fb_cols();
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
			showpage(getcount(poppler_document_get_n_pages(doc)) - 1);
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

int main(int argc, char* argv[])
{
	char *hide = "\x1b[?25l";
	char *show = "\x1b[?25h";
	char *clear = "\x1b[2J";
	if (argc < 2) {
		printf("usage: fbpdf filename\n");
		return 1;
	}
	g_type_init();
	strcpy(filename, argv[1]);
	if (load_document()) {
		printf("cannot open file\n");
		return 1;
	}
	if (poppler_document_get_n_pages(doc)) {
		write(STDIN_FILENO, hide, strlen(hide));
		write(STDOUT_FILENO, clear, strlen(clear));
		printinfo();
		fb_init();
		mainloop();
		cleanup_page();
		fb_free();
		write(STDIN_FILENO, show, strlen(show));
		printf("\n");
	} else {
		printf("zero pages!\n");
		return 1;
	}
	if (doc)
		g_object_unref(G_OBJECT(doc));
	return 0;
}
