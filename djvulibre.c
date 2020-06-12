#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

static void djvu_render(ddjvu_page_t *page, int iw, int ih, void *bitmap)
{
	ddjvu_format_t *fmt;
	ddjvu_rect_t rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = iw;
	rect.h = ih;
	fmt = ddjvu_format_create(DDJVU_FORMAT_RGB24, 0, 0);
	ddjvu_format_set_row_order(fmt, 1);
	memset(bitmap, 0, ih * iw * 3);
	ddjvu_page_render(page, DDJVU_RENDER_COLOR,
				&rect, &rect, fmt, iw * 3, bitmap);
	ddjvu_format_release(fmt);
}

void *doc_draw(struct doc *doc, int p, float zoom, int rotate, int *rows, int *cols)
{
	ddjvu_page_t *page;
	ddjvu_pageinfo_t info;
	int iw, ih, dpi;
	unsigned char *bmp;
	fbval_t *pbuf;
	int i, j;
	page = ddjvu_page_create_by_pageno(doc->doc, p - 1);
	if (!page)
		return NULL;
	while (!ddjvu_page_decoding_done(page))
		if (djvu_handle(doc))
			return NULL;
	if (rotate)
		ddjvu_page_set_rotation(page, (4 - (rotate / 90 % 4)) & 3);
	ddjvu_document_get_pageinfo(doc->doc, p - 1, &info);
	dpi = ddjvu_page_get_resolution(page);
	iw = ddjvu_page_get_width(page) * zoom * 10 / dpi;
	ih = ddjvu_page_get_height(page) * zoom * 10 / dpi;
	if (!(bmp = malloc(ih * iw * 3))) {
		ddjvu_page_release(page);
		return NULL;
	}
	djvu_render(page, iw, ih, bmp);
	ddjvu_page_release(page);
	if (!(pbuf = malloc(ih * iw * sizeof(pbuf[0])))) {
		free(bmp);
		return NULL;
	}
	for (i = 0; i < ih; i++) {
		unsigned char *src = bmp + i * iw * 3;
		fbval_t *dst = pbuf + i * iw;
		for (j = 0; j < iw; j++)
			dst[j] = FB_VAL(src[j * 3], src[j * 3 + 1], src[j * 3 + 2]);
	}
	free(bmp);
	*cols = iw;
	*rows = ih;
	return pbuf;
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
	while (!ddjvu_document_decoding_done(doc->doc))
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
