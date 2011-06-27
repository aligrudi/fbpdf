PREFIX = .
CC = cc
CFLAGS = -Wall -O2 -I$(PREFIX)/include
LDFLAGS = -L$(PREFIX)/lib

all: fbpdf
.c.o:
	$(CC) -c $(CFLAGS) $<
fbpdf: fbpdf.o pdf.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS) -lmupdf -lfitz -lfreetype -ljbig2dec -ljpeg -lz -lopenjpeg -lm
fbdjvu: fbpdf.o djvu.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS) -ldjvulibre -ljpeg -lm -lstdc++
clean:
	-rm -f *.o fbpdf fbdjvu
