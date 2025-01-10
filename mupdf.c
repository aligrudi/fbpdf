#include <stdlib.h>
#include <string.h>
#include "mupdf/fitz.h"
#include "doc.h"

#define MIN_(a, b)	((a) < (b) ? (a) : (b))

struct doc {
	fz_context *ctx;
	fz_document *pdf;
};

void *doc_draw(struct doc *doc, int page, int zoom, int rotate, int bpp, int *rows, int *cols)
{
	fz_matrix ctm;
	fz_pixmap *pix;
	char *pbuf;
	int x, y;
	ctm = fz_scale((float) zoom / 100, (float) zoom / 100);
	ctm = fz_pre_rotate(ctm, rotate);
	pix = fz_new_pixmap_from_page_number(doc->ctx, doc->pdf,
			page - 1, ctm, fz_device_rgb(doc->ctx), 0);
	if (!pix)
		return NULL;
	if (!(pbuf = malloc(pix->w * pix->h * bpp))) {
		fz_drop_pixmap(doc->ctx, pix);
		return NULL;
	}
	for (y = 0; y < pix->h; y++) {
		unsigned char *s = &pix->samples[y * pix->stride];
		char *d = pbuf + (y * pix->w) * bpp;
		for (x = 0; x < pix->w; x++)
			fb_set(d + x * bpp, s[x * pix->n], s[x * pix->n + 1], s[x * pix->n + 2]);
	}
	fz_drop_pixmap(doc->ctx, pix);
	*cols = pix->w;
	*rows = pix->h;
	return pbuf;
}

int doc_pages(struct doc *doc)
{
	return fz_count_pages(doc->ctx, doc->pdf);
}

struct doc *doc_open(char *path)
{
	struct doc *doc = malloc(sizeof(*doc));
	doc->ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	fz_register_document_handlers(doc->ctx);
	fz_try (doc->ctx) {
		doc->pdf = fz_open_document(doc->ctx, path);
	} fz_catch (doc->ctx) {
		fz_drop_context(doc->ctx);
		free(doc);
		return NULL;
	}
	return doc;
}

void doc_close(struct doc *doc)
{
	fz_drop_document(doc->ctx, doc->pdf);
	fz_drop_context(doc->ctx);
	free(doc);
}
