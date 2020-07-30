include config.mk

all: config.h tlsrp

config.h:
	cp config.def.h $@

.c.o:
	$(CC) $(CFLAGS) -c $<

tlsrp: $(OBJ) config.h
	$(CC) $(FLAGS) $(OBJ) -o $@

certs:
	cd CA ; ./certgen.sh

clean:
	rm -f $(OBJ) tlsrp

test: tlsrp certs
	LD_LIBRARY_PATH=$(LIB_PATH) ./tlsrp -U "/tmp/conn.socket" -P 8000 -ca "CA/root.crt" -cert "CA/tlsrp.crt" -key "CA/tlsrp.key"
