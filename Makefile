LIBXML_CFLAGS=`pkg-config libxml-2.0 --cflags`
LIBXML_LIBS=`pkg-config libxml-2.0 --libs`
SSL_LIBS=`pkg-config openssl --libs`
SSL_CFLAGS=`pkg-config openssl --cflags`
SSL=handler-ssl.c $(SSL_LIBS) $(SSL_CFLAGS)
MINCRYPT=-lmincrypt -DUSE_MINCRYPT
READLINE=-lhistory -lreadline -DUSE_READLINE
OBJECTS=cdv-main.c utils.c config.c definitions.c modules.c xml.c database.c database-internal.c sockets.c mincrypt-wrap.c handler-http.c cdvshell.c

all:
	$(CC) -Wall -g -o cdvws $(OBJECTS) -ldl -lrt $(SSL) $(LIBXML_CFLAGS) $(LIBXML_LIBS) $(MINCRYPT) $(READLINE)
