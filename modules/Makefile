CC=gcc
OUTDIR=../bindir/modules
MYSQL_LIBPATH=-L/usr/lib64/mysql
MYSQL_LIBS=-lmysqlclient

all:	modtest

modtest:
	mkdir -p $(OUTDIR)
	$(CC) -c -fPIC mod_test.c
	$(CC) -shared -fPIC -o $(OUTDIR)/mod_test.so mod_test.o
	rm -f *.o

moddbmysql:
	mkdir -p $(OUTDIR)
	$(CC) -c -fPIC mod_db_mysql.c
	$(CC) -shared -fPIC -o $(OUTDIR)/mod_db_mysql.so mod_db_mysql.o $(MYSQL_LIBPATH) $(MYSQL_LIBS)
	rm -f *.o

clean:
	rm -rf $(BINARY)
