LIBXML_CFLAGS=`pkg-config libxml-2.0 --cflags`
LIBXML_LIBS=`pkg-config libxml-2.0 --libs`
SSL_LIBS=`pkg-config openssl --libs`
SSL_CFLAGS=`pkg-config openssl --cflags`
SSL=$(SSL_LIBS) $(SSL_CFLAGS) ssl-server.c
OBJECTS=cdv-main.c utils.c config.c definitions.c modules.c xml.c database.c database-internal.c sockets.c

all:
	$(CC) -Wall -g -o cdvws $(OBJECTS) -ldl -lrt $(LIBXML_CFLAGS) $(LIBXML_LIBS) $(SSL)
