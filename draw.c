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
#define MAXFBWIDTH	(1 << 12)
#define BPP		sizeof(fbval_t)
#define NLEVELS		(1 << 8)

static int fd;
static unsigned char *fb;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int rl, rr, gl, gr, bl, br;
static int nr, ng, nb;

static int fb_len(void)
{
	return finfo.line_length * vinfo.yres_virtual;
}

static void fb_cmap_save(int save)
{
	static unsigned short red[NLEVELS], green[NLEVELS], blue[NLEVELS];
	struct fb_cmap cmap;
	if (finfo.visual == FB_VISUAL_TRUECOLOR)
		return;
	cmap.start = 0;
	cmap.len = MAX(nr, MAX(ng, nb));
	cmap.red = red;
	cmap.green = green;
	cmap.blue = blue;
	cmap.transp = 0;
	ioctl(fd, save ? FBIOGETCMAP : FBIOPUTCMAP, &cmap);
}

void fb_cmap(void)
{
	unsigned short red[NLEVELS], green[NLEVELS], blue[NLEVELS];
	struct fb_cmap cmap;
	int i;
	if (finfo.visual == FB_VISUAL_TRUECOLOR)
		return;

	for (i = 0; i < nr; i++)
		red[i] = (65535 / (nr - 1)) * i;
	for (i = 0; i < ng; i++)
		green[i] = (65535 / (ng - 1)) * i;
	for (i = 0; i < nb; i++)
		blue[i] = (65535 / (nb - 1)) * i;

	cmap.start = 0;
	cmap.len = MAX(nr, MAX(ng, nb));
	cmap.red = red;
	cmap.green = green;
	cmap.blue = blue;
	cmap.transp = 0;

	ioctl(fd, FBIOPUTCMAP, &cmap);
}

static void xerror(char *msg)
{
	perror(msg);
	exit(1);
}

static void xdie(char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

static void init_colors(void)
{
	nr = 1 << vinfo.red.length;
	ng = 1 << vinfo.green.length;
	nb = 1 << vinfo.blue.length;
	rr = 8 - vinfo.red.length;
	rl = vinfo.red.offset;
	gr = 8 - vinfo.green.length;
	gl = vinfo.green.offset;
	br = 8 - vinfo.blue.length;
	bl = vinfo.blue.offset;
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
	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
	init_colors();
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

void fb_free(void)
{
	fb_cmap_save(0);
	munmap(fb, fb_len());
	close(fd);
}

fbval_t fb_color(unsigned char r, unsigned char g, unsigned char b)
{
	return ((r >> rr) << rl) | ((g >> gr) << gl) | ((b >> br) << bl);
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

static unsigned long cache[MAXFBWIDTH];
void fb_box(int sr, int sc, int er, int ec, fbval_t val)
{
	int i;
	int pc = sizeof(cache[0]) / sizeof(val);
	int cn = MIN((ec - sc) / pc + 1, MAXFBWIDTH);
	unsigned long nv = val;
	for (i = 1; i < pc; i++)
		nv = (nv << (sizeof(val) * 8)) | val;
	for (i = 0; i < cn; i++)
		cache[i] = nv;
	for (i = sr; i < er; i++)
		memcpy(rowaddr(i) + sc * BPP, cache, (ec - sc) * BPP);
}
