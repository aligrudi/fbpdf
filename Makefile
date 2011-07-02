PREFIX = .
CC = cc
CFLAGS = -Wall -O2 -I$(PREFIX)/include
LDFLAGS = -L$(PREFIX)/lib

all: fbpdf fbdjvu
%.o: %.c
	$(CC) -c $(CFLAGS) $<
clean:
	-rm -f *.o fbpdf fbdjvu fbpdf2

# pdf support using mupdf
fbpdf: fbpdf.o mupdf.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS) -lmupdf -lfitz -lfreetype \
			-ljbig2dec -ljpeg -lz -lopenjpeg -lm
# djvu support
fbdjvu: fbpdf.o djvulibre.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS) -ldjvulibre -ljpeg -lm -lstdc++

# pdf support using poppler
poppler.o: poppler.c
	$(CC) -c $(CFLAGS) `pkg-config --cflags poppler-glib` $<
fbpdf2: fbpdf.o poppler.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS) `pkg-config --libs poppler-glib`
