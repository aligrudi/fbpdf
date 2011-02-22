struct doc *doc_open(char *path);
int doc_pages(struct doc *doc);
int doc_draw(struct doc *doc, fbval_t *bitmap, int page, int rows, int cols, int zoom);
void doc_close(struct doc *doc);
