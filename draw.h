typedef unsigned short fbval_t;
#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#endif

void fb_init(void);
void fb_free(void);
fbval_t fb_color(unsigned char r, unsigned char g, unsigned char b);
void fb_set(int r, int c, fbval_t *mem, int len);
int fb_rows(void);
int fb_cols(void);
void fb_box(int sr, int sc, int er, int ec, fbval_t val);
void fb_cmap(void);
