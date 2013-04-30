LIBXML_CFLAGS=`pkg-config libxml-2.0 --cflags`
LIBXML_LIBS=`pkg-config libxml-2.0 --libs`
SSL_LIBS=`pkg-config openssl --libs`
SSL_CFLAGS=`pkg-config openssl --cflags`
SSL=handler-ssl.c $(SSL_LIBS) $(SSL_CFLAGS)
MINCRYPT=-lmincrypt -DUSE_MINCRYPT
READLINE=-lhistory -lreadline -DUSE_READLINE
REGEX=-lpcre -DUSE_PCRE
KRB5GSSAPI=-lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err -DUSE_KERBEROS
#MYSQL=-O2 `mysql_config --libs` `mysql_config --cflags` -DUSE_MYSQL
MYSQL=-L/usr/lib64/mysql -lmysqlclient -DUSE_MYSQL
GEOIP_TEST=-DGEOIP_LOCAL_OVERRIDE=3675615582
GEOIP=-lGeoIP -DUSE_GEOIP $(GEOIP_TEST)
#KRB5DEBUG=-DAUTH_MSG_SLEEP
DEBUG=-g $(KRB5DEBUG)
PTHREADS=-lpthread -DUSE_THREADS
NOTIFIER=-DUSE_NOTIFIER
OBJECTS=cdv-main.c utils.c config.c definitions.c modules.c xml.c database.c database-internal.c sockets.c mincrypt-wrap.c handler-http.c cdvshell.c xmlrpc.c variables.c scripting.c regex.c handler-auth.c base64.c threads.c json.c notifier.c crc32.c help.c

all:
	@rm -rf ./bindir
	@mkdir -p ./bindir/modules
	$(CC) -Wall $(DEBUG) -o ./bindir/cdvws $(OBJECTS) -ldl -rdynamic -lrt $(SSL) $(LIBXML_CFLAGS) $(LIBXML_LIBS) $(MINCRYPT) $(READLINE) $(REGEX) $(KRB5GSSAPI) $(MYSQL) $(GEOIP) $(PTHREADS) $(NOTIFIER)
	@cp -af examples bindir
	@cd modules && make && cd -

clean:
	rm -f cdvws
	rm -rf bindir
