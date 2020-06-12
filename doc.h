/* framebuffer depth */
typedef unsigned int fbval_t;

/* optimized version of fb_val() */
#define FB_VAL(r, g, b)	fb_val((r), (g), (b))

struct doc *doc_open(char *path);
int doc_pages(struct doc *doc);
void *doc_draw(struct doc *doc, int page, float zoom, int rotate, int *rows, int *cols);
void doc_close(struct doc *doc);
