#include <stdlib.h>
#include <string.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-image.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>

extern "C" {
#include "doc.h"
}

struct doc {
	poppler::document *doc;
};

static poppler::rotation_enum rotation(int times)
{
	if (times == 1)
		return poppler::rotate_90;
	if (times == 2)
		return poppler::rotate_180;
	if (times == 3)
		return poppler::rotate_270;
	return poppler::rotate_0;
}

int doc_draw(struct doc *doc, int p, int zoom, int rotate,
		fbval_t *bitmap, int *rows, int *cols)
{
	poppler::page *page = doc->doc->create_page(p - 1);
	poppler::page_renderer pr;
	int x, y;
	int h, w;
	unsigned char *dat;
	pr.set_render_hint(poppler::page_renderer::antialiasing, true);
	pr.set_render_hint(poppler::page_renderer::text_antialiasing, true);
	poppler::image img = pr.render_page(page, 72 * zoom / 10, 72 * zoom / 10,
				-1, -1, -1, -1, rotation((rotate + 89) / 90));
	h = img.height();
	w = img.width();
	dat = (unsigned char *) img.data();
	for (y = 0; y < h; y++) {
		int xs = y * *cols + (*cols - w) / 2;
		for (x = 0; x < w; x++) {
			unsigned char *s = dat + img.bytes_per_row() * y + x * 4;
			bitmap[xs + x] = FB_VAL(s[2], s[1], s[0]);
		}
	}
	*rows = h;
	*cols = w;
	delete page;
	return 0;
}

int doc_pages(struct doc *doc)
{
	return doc->doc->pages();
}

struct doc *doc_open(char *path)
{
	struct doc *doc = (struct doc *) malloc(sizeof(*doc));
	doc->doc = poppler::document::load_from_file(path);
	if (!doc->doc) {
		doc_close(doc);
		return NULL;
	}
	return doc;
}

void doc_close(struct doc *doc)
{
	delete doc->doc;
	free(doc);
}
