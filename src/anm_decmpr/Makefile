CC := gcc
CFLAGS :=
INSTALL := install
PREFIX := ../..
bindir := $(PREFIX)/bin

anm: main.c anm_decompress.c anm_create.c util.c anm_decompress.h anm_create.h util.h qdbmp.c qdbmp.h
	$(CC) $(CFLAGS) -Wall main.c anm_decompress.c anm_create.c util.c qdbmp.c -o $@ 


.PHONY: all clean install

all: anm

install: anm
	$(INSTALL) -d $(bindir)
	$(INSTALL) anm $(bindir)

clean:
	rm -f anm
