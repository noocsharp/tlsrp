include config.mk

all: config.h tlsrp

config.h:
	cp config.def.h $@

.c.o:
	$(CC) $(CFLAGS) -c $< $(FLAGS)

tlsrp: $(OBJ) config.h
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(FLAGS)

clean:
	rm -f $(OBJ) tlsrp

test: tlsrp
	LD_LIBRARY_PATH=/lib/libressl ./tlsrp -U "/tmp/conn.socket" -f 443 -a "CA/root.pem" -r "CA/tlsrp.crt" -k "CA/tlsrp.key"
