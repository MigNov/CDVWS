LIBXML_CFLAGS=`pkg-config libxml-2.0 --cflags`
LIBXML_LIBS=`pkg-config libxml-2.0 --libs`
SSL_LIBS=`pkg-config openssl --libs`
SSL_CFLAGS=`pkg-config openssl --cflags`
SSL=handler-ssl.c $(SSL_LIBS) $(SSL_CFLAGS)
MINCRYPT=-lmincrypt -DUSE_MINCRYPT
READLINE=-lhistory -lreadline -DUSE_READLINE
REGEX=-lpcre -DUSE_PCRE
OBJECTS=cdv-main.c utils.c config.c definitions.c modules.c xml.c database.c database-internal.c sockets.c mincrypt-wrap.c handler-http.c cdvshell.c xmlrpc.c variables.c scripting.c regex.c

all:
	@rm -rf ./bindir
	@mkdir -p ./bindir/modules
	$(CC) -Wall -g -o ./bindir/cdvws $(OBJECTS) -ldl -rdynamic -lrt $(SSL) $(LIBXML_CFLAGS) $(LIBXML_LIBS) $(MINCRYPT) $(READLINE) $(REGEX)
	@cp -af examples bindir
	@cd modules && make && cd -

clean:
	rm -f cdvws
	rm -rf bindir
