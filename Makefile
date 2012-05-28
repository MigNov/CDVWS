LIBXML_CFLAGS=`pkg-config libxml-2.0 --cflags`
LIBXML_LIBS=`pkg-config libxml-2.0 --libs`

all:
	$(CC) -Wall -g -o cdvws cdv-main.c utils.c config.c definitions.c modules.c xml.c database.c database-internal.c -ldl -lrt $(LIBXML_CFLAGS) $(LIBXML_LIBS)
