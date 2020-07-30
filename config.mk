CC = cc
CFLAGS = -std=c99 -Wall -Wextra

# Arch Linux Testing
LIB_PATH=/lib/libressl
LIBTLS_PKGCONF_PATH = /usr/lib/libressl/pkgconfig/
CFLAGS := $(CFLAGS) `PKG_CONFIG_PATH=$(LIBTLS_PKGCONF_PATH) pkg-config --cflags libtls`
FLAGS = `PKG_CONFIG_PATH=$(LIBTLS_PKGCONF_PATH) pkg-config --libs libtls`

# glibc
CFLAGS := $(CFLAGS) -D_XOPEN_SOURCE=700

# Any system that is using LibreSSL as the default SSL provider
# FLAGS := $(FLAGS) -ltls

SRC = tlsrp.c util.c
OBJ = $(SRC:.c=.o)
