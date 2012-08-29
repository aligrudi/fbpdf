#include <stdlib.h>
#include <string.h>
#include "fitz.h"
#include "draw.h"
#include "doc.h"

#define MIN_(a, b)	((a) < (b) ? (a) : (b))

struct doc {
	fz_context *ctx;
	fz_document *pdf;
};

int doc_draw(struct doc *doc, int p, int zoom, int rotate,
		fbval_t *bitmap, int *rows, int *cols)
{
	fz_matrix ctm;
	fz_bbox bbox;
	fz_pixmap *pix;
	fz_device *dev;
	fz_page *page;
	fz_rect rect;
	int h, w;
	int x, y;

	if (!(page = fz_load_page(doc->pdf, p - 1)))
		return 1;
	ctm = fz_scale((float) zoom / 10, (float) -zoom / 10);
	ctm = fz_concat(ctm, fz_translate(0, -100));
	if (rotate)
		ctm = fz_concat(ctm, fz_rotate(rotate));
	rect = fz_bound_page(doc->pdf, page);
	rect = fz_transform_rect(ctm, rect);
	bbox = fz_round_rect(rect);
	w = MIN_(*cols, rect.x1 - rect.x0);
	h = MIN_(*rows, rect.y1 - rect.y0);

	pix = fz_new_pixmap_with_bbox(doc->ctx, fz_device_rgb, bbox);
	fz_clear_pixmap_with_value(doc->ctx, pix, 0xff);

	dev = fz_new_draw_device(doc->ctx, pix);
	fz_run_page(doc->pdf, page, dev, ctm, NULL);
	fz_free_device(dev);

	for (y = 0; y < h; y++) {
		int xs = (h - y - 1) * *cols + (*cols - w) / 2;
		for (x = 0; x < w; x++) {
			unsigned char *s = fz_pixmap_samples(doc->ctx, pix) +
					y * fz_pixmap_width(doc->ctx, pix) * 4 + x * 4;
			bitmap[xs + x] = FB_VAL(s[0], s[1], s[2]);

		}
	}
	fz_drop_pixmap(doc->ctx, pix);
	fz_free_page(doc->pdf, page);
	*cols = w;
	*rows = h;
	return 0;
}

int doc_pages(struct doc *doc)
{
	return fz_count_pages(doc->pdf);
}

struct doc *doc_open(char *path)
{
	struct doc *doc = malloc(sizeof(*doc));
	doc->ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	doc->pdf = fz_open_document(doc->ctx, path);
	if (!doc->pdf || !fz_count_pages(doc->pdf)) {
		free(doc);
		return NULL;
	}
	return doc;
}

void doc_close(struct doc *doc)
{
	fz_close_document(doc->pdf);
	fz_free_context(doc->ctx);
	free(doc);
}
