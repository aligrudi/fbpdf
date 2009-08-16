#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "draw.h"

#define FBDEV_PATH	"/dev/fb0"
#define MAXWIDTH	(1 << 12)
#define BPP		sizeof(fbval_t)
#define NLEVELS		(1 << 8)
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

static void xerror(char *msg)
{
	perror(msg);
	exit(1);
}

static int fd;
static unsigned char *fb;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;

static int fb_len()
{
	return vinfo.xres_virtual * vinfo.yres_virtual * BPP;
}

static void fb_cmap_save(int save)
{
	static unsigned short red[NLEVELS], green[NLEVELS], blue[NLEVELS];
	struct fb_cmap cmap;
	int mr = 1 << vinfo.red.length;
	int mg = 1 << vinfo.green.length;
	int mb = 1 << vinfo.blue.length;
	if (finfo.visual == FB_VISUAL_TRUECOLOR)
		return;
	cmap.start = 0;
	cmap.len = MAX(mr, MAX(mg, mb));
	cmap.red = red;
	cmap.green = green;
	cmap.blue = blue;
	cmap.transp = 0;
	if (save)
		ioctl(fd, FBIOGETCMAP, &cmap);
	else
		ioctl(fd, FBIOPUTCMAP, &cmap);
}

void fb_cmap(void)
{
	unsigned short red[NLEVELS], green[NLEVELS], blue[NLEVELS];
	struct fb_cmap cmap;
	int mr = 1 << vinfo.red.length;
	int mg = 1 << vinfo.green.length;
	int mb = 1 << vinfo.blue.length;
	int i;
	if (finfo.visual == FB_VISUAL_TRUECOLOR)
		return;

	for (i = 0; i < mr; i++)
		red[i] = (65535 / (mr - 1)) * i;
	for (i = 0; i < mg; i++)
		green[i] = (65535 / (mg - 1)) * i;
	for (i = 0; i < mb; i++)
		blue[i] = (65535 / (mb - 1)) * i;

	cmap.start = 0;
	cmap.len = MAX(mr, MAX(mg, mb));
	cmap.red = red;
	cmap.green = green;
	cmap.blue = blue;
	cmap.transp = 0;

	ioctl(fd, FBIOPUTCMAP, &cmap);
}

static void xdie(char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

void fb_init(void)
{
	fd = open(FBDEV_PATH, O_RDWR);
	if (fd == -1)
		xerror("can't open " FBDEV_PATH);
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
		xerror("ioctl failed");
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
		xerror("ioctl failed");
	if ((vinfo.bits_per_pixel + 7) >> 3 != BPP)
		xdie("fbval_t does not match framebuffer depth");
	fb = mmap(NULL, fb_len(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED)
		xerror("can't map the framebuffer");
	fb_cmap_save(1);
	fb_cmap();
}

void fb_set(int r, int c, fbval_t *mem, int len)
{
	long loc = (c + vinfo.xoffset) * BPP +
		(r + vinfo.yoffset) * finfo.line_length;
	memcpy(fb + loc, mem, len * BPP);
}

void fb_free()
{
	fb_cmap_save(0);
	munmap(fb, fb_len());
	close(fd);
}

static fbval_t color_bits(struct fb_bitfield *bf, fbval_t v)
{
	fbval_t moved = v >> (8 - bf->length);
	return moved << bf->offset;
}

fbval_t fb_color(unsigned char r, unsigned char g, unsigned char b)
{
	return color_bits(&vinfo.red, r) |
		color_bits(&vinfo.green, g) |
		color_bits(&vinfo.blue, b);
}

int fb_rows(void)
{
	return vinfo.yres;
}

int fb_cols(void)
{
	return vinfo.xres;
}

static unsigned char *rowaddr(int r)
{
	return fb + (r + vinfo.yoffset) * finfo.line_length;
}

static unsigned long cache[MAXWIDTH];
void fb_box(int sr, int sc, int er, int ec, fbval_t val)
{
	int i;
	int pc = sizeof(cache[0]) / sizeof(val);
	int cn = MIN((ec - sc) / pc + 1, ARRAY_SIZE(cache));
	unsigned long nv = val;
	for (i = 1; i < pc; i++)
		nv = (nv << (sizeof(val) * 8)) | val;
	for (i = 0; i < cn; i++)
		cache[i] = nv;
	for (i = sr; i < er; i++)
		memcpy(rowaddr(i) + sc * BPP, cache, (ec - sc) * BPP);
}
