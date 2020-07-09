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

run:
	LD_LIBRARY_PATH=/usr/lib/libressl ./$(OBJ) -U "/tmp/conn.socket" -f 443 -a "/home/nihal/projects/libtls/CA/root.pem" -r "/home/nihal/projects/libtls/CA/server.crt" -k "/home/nihal/projects/libtls/CA/server.key"
