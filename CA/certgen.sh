#!/bin/sh

# This script generates certificates that will be used in "make test".

# generate root key
openssl ecparam -out root.key -name prime256v1 -genkey

# create certificate signing request
openssl req -new -sha256 -key root.key -out root.csr -subj '/CN=localhost'

# sign root key to create certificate
openssl x509 -req -sha256 -days 365 -in root.csr -signkey root.key -out root.crt

# create tlsrp test key
openssl ecparam -out tlsrp.key -name prime256v1 -genkey

# gen csr
openssl req -new -sha256 -key tlsrp.key -out tlsrp.csr -subj '/CN=localhost'

# sign tlsrp key using root key to create cert
openssl x509 -req -in tlsrp.csr -CA  root.crt -CAkey root.key -CAcreateserial -out tlsrp.crt -days 365 -sha256
