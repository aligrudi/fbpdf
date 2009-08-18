#include <ctype.h>
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

static int load_document(char *filename)
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

static void mainloop()
{
	int step = fb_rows() / PAGESTEPS;
	int hstep = fb_cols() / PAGESTEPS;
	struct termios oldtermios, termios;
	int maxhead, maxleft;
	int c;
	tcgetattr(STDIN_FILENO, &termios);
	oldtermios = termios;
	cfmakeraw(&termios);
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios);
	showpage(0);
	while ((c = readkey()) != -1) {
		maxhead = cairo_image_surface_get_height(surface) - fb_rows();
		maxleft = cairo_image_surface_get_width(surface) - fb_cols();
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
			head += fb_rows() * getcount(1) - step;
			break;
		case '\b':
			head -= fb_rows() * getcount(1) - step;
			break;
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
		case 'q':
			tcsetattr(STDIN_FILENO, 0, &oldtermios);
			return;
		case 27:
			count = 0;
			break;
		default:
			if (isdigit(c))
				count = count * 10 + c - '0';
		}
		head = MAX(0, MIN(maxhead, head));
		left = MAX(0, MIN(maxleft, left));
		draw();
	}
}

int main(int argc, char* argv[])
{
	char *clear = "\x1b[2J\x1b[H";
	char *msg = "\t\t\t\t---FBPDF---\n";
	char *hide = "\x1b[?25l";
	char *show = "\x1b[?25h";
	if (argc < 2) {
		printf("usage: fbpdf filename\n");
		return 1;
	}
	g_type_init();
	if (load_document(argv[1])) {
		printf("cannot open file\n");
		return 1;
	}
	write(STDIN_FILENO, clear, strlen(clear));
	write(STDIN_FILENO, msg, strlen(msg));
	write(STDIN_FILENO, hide, strlen(hide));
	fb_init();
	mainloop();
	cleanup_page();
	fb_free();
	if (doc)
		g_object_unref(G_OBJECT(doc));
	write(STDIN_FILENO, show, strlen(show));
	return 0;
}
