#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "draw.h"

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define MAX(a, b)	((a) > (b) ? (a) : (b))
#define NLEVELS		(1 << 8)

static int fd;
static void *fb;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int bpp;
static int nr, ng, nb;
static int rl, rr, gl, gr, bl, br;	/* fb_color() shifts */

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
	cmap.transp = NULL;
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
	cmap.transp = NULL;

	ioctl(fd, FBIOPUTCMAP, &cmap);
}

unsigned fb_mode(void)
{
	return (bpp << 16) | (vinfo.red.length << 8) |
		(vinfo.green.length << 4) | (vinfo.blue.length);
}

static void init_colors(void)
{
	nr = 1 << vinfo.red.length;
	ng = 1 << vinfo.blue.length;
	nb = 1 << vinfo.green.length;
	rr = 8 - vinfo.red.length;
	rl = vinfo.red.offset;
	gr = 8 - vinfo.green.length;
	gl = vinfo.green.offset;
	br = 8 - vinfo.blue.length;
	bl = vinfo.blue.offset;
}

int fb_init(void)
{
	fd = open(FBDEV_PATH, O_RDWR);
	if (fd == -1)
		goto failed;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
		goto failed;
	if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
		goto failed;
	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
	bpp = (vinfo.bits_per_pixel + 7) >> 3;
	fb = mmap(NULL, fb_len(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fb == MAP_FAILED)
		goto failed;
	init_colors();
	fb_cmap_save(1);
	fb_cmap();
	return 0;
failed:
	perror("fb_init()");
	close(fd);
	return 1;
}

void fb_free(void)
{
	fb_cmap_save(0);
	munmap(fb, fb_len());
	close(fd);
}

int fb_rows(void)
{
	return vinfo.yres;
}

int fb_cols(void)
{
	return vinfo.xres;
}

void *fb_mem(int r)
{
	return fb + (r + vinfo.yoffset) * finfo.line_length;
}

void fb_set(int r, int c, void *mem, int len)
{
	memcpy(fb_mem(r) + (c + vinfo.xoffset) * bpp, mem, len * bpp);
}

unsigned fb_val(int r, int g, int b)
{
	return ((r >> rr) << rl) | ((g >> gr) << gl) | ((b >> br) << bl);
}
