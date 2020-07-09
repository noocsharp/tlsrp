.include "config.mk"

all: config.h tlsrp

config.h:
	cp config.def.h $@

.c.o:
	$(CC) -c $< $(FLAGS)

tlsrp: $(OBJ)
	$(CC) $(OBJ) -o $@ $(FLAGS)

clean:
	rm -f $(OBJ) tlsrp

test: $(BIN)
	LD_LIBRARY_PATH=/usr/lib/libressl ./$(BIN) -U "/tmp/conn.socket" -f 443 -a "CA/root.pem" -r "CA/server.crt" -k "CA/server.key"
