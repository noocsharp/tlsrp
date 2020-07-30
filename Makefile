include config.mk

SRC = tlsrp.c util.c
OBJ = $(SRC:.c=.o)

all: config.h tlsrp

config.h:
	cp config.def.h $@

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

tlsrp: $(OBJ) config.h
	$(CC) $(FLAGS) $(OBJ) -o $@

certs:
	cd CA ; ./certgen.sh

clean:
	rm -f $(OBJ) tlsrp

install: all
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	cp -f tlsrp "$(DESTDIR)$(PREFIX)/bin"
	chmod 755 "$(DESTDIR)$(PREFIX)/bin/tlsrp"
	mkdir -p "$(DESTDIR)$(MANPREFIX)/man1"
	cp tlsrp.1 "$(DESTDIR)$(MANPREFIX)/man1/tlsrp.1"
	chmod 644 "$(DESTDIR)$(MANPREFIX)/man1/tlsrp.1"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/tlsrp"
	rm -f "$(DESTDIR)$(MANPREFIX)/man1/tlsrp.1"

test: tlsrp certs
	LD_LIBRARY_PATH=$(LIB_PATH) ./tlsrp -u "/tmp/conn.socket" -P 8000 -a "CA/root.crt" -r "CA/tlsrp.crt" -k "CA/tlsrp.key"
