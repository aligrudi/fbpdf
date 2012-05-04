#include <stdlib.h>
#include <string.h>
#include <libdjvu/ddjvuapi.h>
#include "draw.h"
#include "doc.h"

#define MIN(a, b)	((a) < (b) ? (a) : (b))

struct doc {
	ddjvu_context_t *ctx;
	ddjvu_document_t *doc;
};

int djvu_handle(struct doc *doc)
{
	ddjvu_message_t *msg;
	msg = ddjvu_message_wait(doc->ctx);
	while ((msg = ddjvu_message_peek(doc->ctx))) {
		if (msg->m_any.tag == DDJVU_ERROR) {
			fprintf(stderr,"ddjvu: %s\n", msg->m_error.message);
			return 1;
		}
		ddjvu_message_pop(doc->ctx);
	}
	return 0;
}

static void djvu_render(ddjvu_page_t *page, int mode, void *bitmap, int cols,
		ddjvu_rect_t *prect, ddjvu_rect_t *rrect)
{
	ddjvu_format_t *fmt;
	fmt = ddjvu_format_create(DDJVU_FORMAT_GREY8, 0, 0);
	ddjvu_format_set_row_order(fmt, 1);

	ddjvu_page_render(page, mode, prect, rrect, fmt, cols, bitmap);
	ddjvu_format_release(fmt);
}

#define SIZE		(1 << 13)
static char img[SIZE * SIZE];

int doc_draw(struct doc *doc, fbval_t *bitmap, int p, int rows, int cols,
		int zoom, int rotate)
{
	ddjvu_page_t *page;
	ddjvu_rect_t rect;
	ddjvu_pageinfo_t info;
	int w, h;
	int i, j;
	page = ddjvu_page_create_by_pageno(doc->doc, p - 1);
	if (!page)
		return -1;
	if (!ddjvu_page_decoding_done(page))
		if (djvu_handle(doc))
			return -1;

	ddjvu_document_get_pageinfo(doc->doc, p - 1, &info);
	rect.x = 0;
	rect.y = 0;
	rect.w = ddjvu_page_get_width(page);
	rect.h = ddjvu_page_get_height(page);

	/* mode: DDJVU_RENDER_(BLACK|COLOR|BACKGROUND|FOREGROUND) */
	djvu_render(page, DDJVU_RENDER_FOREGROUND, img, SIZE, &rect, &rect);
	ddjvu_page_release(page);
	zoom /= 4;
	w = MIN(cols, rect.w * zoom / 10);
	h = MIN(cols, rect.h * zoom / 10);
	for (i = 0; i < h; i++) {
		int xs = i * cols + (cols - w) / 2;
		for (j = 0; j < w; j++)
			bitmap[xs + j] = img[i * 10 / zoom * SIZE + j * 10 / zoom];
	}
	return 0;
}

int doc_pages(struct doc *doc)
{
	return ddjvu_document_get_pagenum(doc->doc);
}

struct doc *doc_open(char *path)
{
	struct doc *doc = malloc(sizeof(*doc));
	doc->ctx = ddjvu_context_create("fbpdf");
	if (!doc->ctx)
		goto fail;
	doc->doc = ddjvu_document_create_by_filename(doc->ctx, path, 1);
	if (!doc->doc)
		goto fail;
	if (!ddjvu_document_decoding_done(doc->doc))
		if (djvu_handle(doc))
			goto fail;
	return doc;
fail:
	doc_close(doc);
	return NULL;
}

void doc_close(struct doc *doc)
{
	if (doc->doc)
		ddjvu_context_release(doc->ctx);
	if (doc->ctx)
		ddjvu_document_release(doc->doc);
	free(doc);
}
