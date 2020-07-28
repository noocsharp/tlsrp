CC = cc
CFLAGS = -std=c99 -Wall -Wextra

# Arch Linux Testing
LIBTLS_PKGCONF_PATH = /usr/lib/libressl/pkgconfig/
FLAGS = `PKG_CONFIG_PATH=$(LIBTLS_PKGCONF_PATH) pkg-config --cflags --libs libtls` -D_XOPEN_SOURCE=700

# OpenBSD
# FLAGS = -ltls

SRC = tlsrp.c util.c
OBJ = $(SRC:.c=.o)
