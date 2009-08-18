CC = cc
CFLAGS = -std=gnu89 -pedantic -Wall -O2 \
	`pkg-config --cflags poppler-glib`
LDFLAGS = `pkg-config --libs poppler-glib`

all: fbpdf
.c.o:
	$(CC) -c $(CFLAGS) $<
fbpdf: fbpdf.o draw.o
	$(CC) $(LDFLAGS) -o $@ $^
clean:
	-rm -f *.o fbpdf
ctags:
	ctags *.[hc]
