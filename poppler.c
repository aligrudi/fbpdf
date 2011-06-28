#include <stdlib.h>
#include <string.h>
#include <cairo/cairo.h>
#include <glib/poppler.h>
#include "doc.h"

struct doc {
	PopplerDocument *doc;
};

int doc_draw(struct doc *doc, fbval_t *bitmap, int p,
		int rows, int cols, int zoom, int rotate)
{
	cairo_t *cairo;
	cairo_surface_t *surface;
	PopplerPage *page;
	unsigned char *img;
	int i, j;
	int h, w;
	int iw;
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cols, rows);
	cairo = cairo_create(surface);
	cairo_scale(cairo, (float) zoom / 10, (float) zoom / 10);
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
	cairo_paint(cairo);
	img = cairo_image_surface_get_data(surface);
	page = poppler_document_get_page(doc->doc, p - 1);
	poppler_page_render(page, cairo);
	iw = cairo_image_surface_get_width(surface);
	h = MIN(rows, cairo_image_surface_get_height(surface));
	w = MIN(cols, iw);
	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			unsigned char *s = img + (i * iw + j) * 4;
			bitmap[i * cols + j] = FB_VAL(*(s + 2), *(s + 1), *s);
		}
	}
	cairo_destroy(cairo);
	cairo_surface_destroy(surface);
	g_object_unref(G_OBJECT(page));
	return 0;
}

int doc_pages(struct doc *doc)
{
	return poppler_document_get_n_pages(doc->doc);
}

struct doc *doc_open(char *path)
{
	struct doc *doc = malloc(sizeof(*doc));
	char abspath[PATH_MAX];
	char uri[PATH_MAX + 16];
	realpath(path, abspath);
	snprintf(uri, sizeof(uri), "file://%s", abspath);
	g_type_init();
	doc->doc = poppler_document_new_from_file(uri, NULL, NULL);
	return doc;
}

void doc_close(struct doc *doc)
{
	g_object_unref(G_OBJECT(doc->doc));
	free(doc);
}
