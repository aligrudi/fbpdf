/* framebuffer depth */
typedef unsigned int fbval_t;

/* optimized version of fb_val() */
#define FB_VAL(r, g, b)	fb_val((r), (g), (b))

struct doc *doc_open(char *path);
int doc_pages(struct doc *doc);
int doc_draw(struct doc *doc, int page, int zoom, int rotate,
		fbval_t *bitmap, int *rows, int *cols);
void doc_close(struct doc *doc);
