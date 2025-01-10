struct doc *doc_open(char *path);
int doc_pages(struct doc *doc);
void *doc_draw(struct doc *doc, int page, int zoom, int rotate, int bpp, int *rows, int *cols);
void doc_close(struct doc *doc);

void fb_set(char *d, unsigned r, unsigned g, unsigned b);
