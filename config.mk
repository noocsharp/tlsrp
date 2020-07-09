CC = cc

LIBTLS_PKGCONF_PATH = /usr/lib/libressl/pkgconfig/
FLAGS = `PKG_CONFIG_PATH=$(LIBTLS_PKGCONF_PATH) pkg-config --cflags --libs libtls`

SRC = tlsrp.c util.c
OBJ = $(SRC:.c=.o)
