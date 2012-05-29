LIBXML_CFLAGS=`pkg-config libxml-2.0 --cflags`
LIBXML_LIBS=`pkg-config libxml-2.0 --libs`
SSL_LIBS=`pkg-config openssl --libs`
SSL_CFLAGS=`pkg-config openssl --cflags`
SSL=ssl-server.c $(SSL_LIBS) $(SSL_CFLAGS)
MINCRYPT=-lmincrypt -DUSE_MINCRYPT
OBJECTS=cdv-main.c utils.c config.c definitions.c modules.c xml.c database.c database-internal.c sockets.c mincrypt-wrap.c http-handler.c

all:
	$(CC) -Wall -g -o cdvws $(OBJECTS) -ldl -lrt $(SSL) $(LIBXML_CFLAGS) $(LIBXML_LIBS) $(MINCRYPT)
