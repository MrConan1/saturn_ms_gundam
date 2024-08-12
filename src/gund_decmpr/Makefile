CC := gcc
CFLAGS :=
INSTALL := install
PREFIX := ../..
bindir := $(PREFIX)/bin

gund_decmpr: main.c gundam_decompress.c qdbmp.c util.c gundam_extract.c gundam_decompress.h qdbmp.h util.h gundam_extract.h
	$(CC) $(CFLAGS) -Wall main.c gundam_decompress.c qdbmp.c util.c gundam_extract.c -o $@ 


.PHONY: all clean install

all: gund_decmpr

install: gund_decmpr
	$(INSTALL) -d $(bindir)
	$(INSTALL) gund_decmpr $(bindir)

clean:
	rm -f gund_decmpr
