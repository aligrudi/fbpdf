MUPDF_PATH = .
CC = cc
CFLAGS = -Wall -Os -I$(MUPDF_PATH)/include
LDFLAGS = -lm -L$(MUPDF_PATH)/lib -lmupdf -lfreetype -ljbig2dec -ljpeg -lz -lopenjpeg

all: fbpdf
.c.o:
	$(CC) -c $(CFLAGS) $<
fbpdf: fbpdf.o doc.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS)
clean:
	-rm -f *.o fbpdf
