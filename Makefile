PREFIX = .
CC = cc
CFLAGS = -Wall -O2 -I$(PREFIX)/include
LDFLAGS = -L$(PREFIX)/lib
PKG_CONFIG = pkg-config

all: fbpdf fbdjvu
%.o: %.c doc.h
	$(CC) -c $(CFLAGS) $<
clean:
	-rm -f *.o fbpdf fbdjvu fbpdf2

# pdf support using mupdf
fbpdf: fbpdf.o mupdf.o draw.o
	$(CC) -o $@ $^ $(LDFLAGS) $(shell $(PKG_CONFIG) --libs mupdf) -lm

# djvu support
fbdjvu: fbpdf.o djvulibre.o draw.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(shell $(PKG_CONFIG) --libs ddjvuapi libjpeg) -lm -lpthread

# pdf support using poppler
poppler.o: poppler.c
	$(CXX) -c $(CFLAGS) $(shell $(PKG_CONFIG) --cflags poppler-cpp) $<
fbpdf2: fbpdf.o poppler.o draw.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(shell $(PKG_CONFIG) --libs poppler-cpp)
