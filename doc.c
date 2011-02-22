#include <stdlib.h>
#include <string.h>
#include "fitz.h"
#include "mupdf.h"
#include "draw.h"
#include "doc.h"

struct doc {
	fz_glyphcache *glyphcache;
	pdf_xref *xref;
};

int doc_draw(struct doc *doc, fbval_t *bitmap, int p, int rows, int cols, int zoom)
{
	fz_matrix ctm;
	fz_bbox bbox;
	fz_pixmap *pix;
	fz_device *dev;
	fz_obj *pageobj;
	fz_displaylist *list;
	pdf_page *page;
	int x, y, w, h;

	pageobj = pdf_getpageobject(doc->xref, p);
	if (pdf_loadpage(&page, doc->xref, pageobj))
		return 1;
	list = fz_newdisplaylist();
	dev = fz_newlistdevice(list);
	if (pdf_runpage(doc->xref, page, dev, fz_identity))
		return 1;
	fz_freedevice(dev);

	ctm = fz_translate(0, -page->mediabox.y1);
	ctm = fz_concat(ctm, fz_scale((float) zoom / 10, (float) -zoom / 10));
	bbox = fz_roundrect(fz_transformrect(ctm, page->mediabox));
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	pix = fz_newpixmapwithrect(fz_devicergb, bbox);
	fz_clearpixmap(pix, 0xff);

	dev = fz_newdrawdevice(doc->glyphcache, pix);
	fz_executedisplaylist(list, dev, ctm);
	fz_freedevice(dev);

	for (y = 0; y < MIN(pix->h, rows); y++) {
		for (x = 0; x < MIN(pix->w, cols); x++) {
			unsigned char *s = pix->samples + y * pix->w * 4 + x * 4;
			bitmap[y * cols + x] = fb_color(s[0], s[1], s[2]);

		}
	}
	fz_droppixmap(pix);
	fz_freedisplaylist(list);
	pdf_freepage(page);
	pdf_agestore(doc->xref->store, 3);
	return 0;
}

int doc_pages(struct doc *doc)
{
	return pdf_getpagecount(doc->xref);
}

struct doc *doc_open(char *path)
{
	struct doc *doc = malloc(sizeof(*doc));
	fz_accelerate();
	doc->glyphcache = fz_newglyphcache();
	if (pdf_openxref(&doc->xref, path, NULL)) {
		free(doc);
		return NULL;
	}
	if (pdf_loadpagetree(doc->xref)) {
		free(doc);
		return NULL;
	}
	return doc;
}

void doc_close(struct doc *doc)
{
	pdf_freexref(doc->xref);
	fz_freeglyphcache(doc->glyphcache);
	free(doc);
}
