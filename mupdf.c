#include <stdlib.h>
#include <string.h>
#include "fitz.h"
#include "mupdf.h"
#include "draw.h"
#include "doc.h"

struct doc {
	fz_glyph_cache *glyphcache;
	pdf_xref *xref;
};

int doc_draw(struct doc *doc, fbval_t *bitmap, int p, int rows, int cols, int zoom, int rotate)
{
	fz_matrix ctm;
	fz_bbox bbox;
	fz_pixmap *pix;
	fz_device *dev;
	fz_display_list *list;
	pdf_page *page;
	int x, y;

	if (pdf_load_page(&page, doc->xref, p - 1))
		return 1;
	list = fz_new_display_list();
	dev = fz_new_list_device(list);
	if (pdf_run_page(doc->xref, page, dev, fz_identity))
		return 1;
	fz_free_device(dev);

	ctm = fz_translate(0, -page->mediabox.y1);
	ctm = fz_concat(ctm, fz_scale((float) zoom / 10, (float) -zoom / 10));
	if (rotate)
		ctm = fz_concat(ctm, fz_rotate(rotate));
	bbox = fz_round_rect(fz_transform_rect(ctm, page->mediabox));

	pix = fz_new_pixmap_with_rect(fz_device_rgb, bbox);
	fz_clear_pixmap_with_color(pix, 0xff);

	dev = fz_new_draw_device(doc->glyphcache, pix);
	fz_execute_display_list(list, dev, ctm, bbox);
	fz_free_device(dev);

	for (y = 0; y < MIN(pix->h, rows); y++) {
		for (x = 0; x < MIN(pix->w, cols); x++) {
			unsigned char *s = pix->samples + y * pix->w * 4 + x * 4;
			bitmap[y * cols + x] = FB_VAL(s[0], s[1], s[2]);

		}
	}
	fz_drop_pixmap(pix);
	fz_free_display_list(list);
	pdf_free_page(page);
	pdf_age_store(doc->xref->store, 3);
	return 0;
}

int doc_pages(struct doc *doc)
{
	return pdf_count_pages(doc->xref);
}

struct doc *doc_open(char *path)
{
	struct doc *doc = malloc(sizeof(*doc));
	fz_accelerate();
	doc->glyphcache = fz_new_glyph_cache();
	if (pdf_open_xref(&doc->xref, path, NULL)) {
		free(doc);
		return NULL;
	}
	if (pdf_load_page_tree(doc->xref)) {
		free(doc);
		return NULL;
	}
	return doc;
}

void doc_close(struct doc *doc)
{
	pdf_free_xref(doc->xref);
	fz_free_glyph_cache(doc->glyphcache);
	free(doc);
}
