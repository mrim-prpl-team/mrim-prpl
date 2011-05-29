ifndef PODIR
PODIR=po/
endif

ifndef PRJID
PRJID=mrim-prpl-underbush
endif

ifndef POPRJ
POPRJ=${PODIR}${PRJID}
endif

ifndef CFLAGS
CFLAGS=`pkg-config purple gtk+-2.0 --cflags` -fPIC -DPIC
endif

ifndef LDFLAGS
LDFLAGS=`pkg-config purple gtk+-2.0 --libs` -shared -ggdb -fPIC -DPIC
endif

all: compile i18n
clean:
	rm -f *.so
	rm -f *.o
	rm -f ${PODIR}*.mo
install:
	install -Dm0755 mrim.so /usr/lib/purple-2/mrim.so
	install -Dm0644 pixmaps/mrim16.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/16/mrim.png
	install -Dm0644 pixmaps/mrim22.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/22/mrim.png
	install -Dm0644 pixmaps/mrim48.png  ${DESTDIR}/usr/share/pixmaps/pidgin/protocols/48/mrim.png
	install -Dm0644 ${POPRJ}-ru_RU.mo ${DESTDIR}/usr/share/locale/ru/LC_MESSAGES/${PRJID}.mo
	install -Dm0644 ${POPRJ}-ru_RU.mo ${DESTDIR}/usr/share/locale/ru_RU/LC_MESSAGES/${PRJID}.mo
uninstall:
	rm -f /usr/lib/purple-2/mrim.so
compile: mrim.o package.o statuses.o cl.o message.o util.o
	gcc ${LDFLAGS} -o mrim.so mrim.o package.o statuses.o cl.o message.o util.o
mrim.o: mrim.c mrim.h statuses.h cl.h message.h package.h config.h
	gcc -c ${CFLAGS} -o mrim.o mrim.c
package.o: package.c mrim.h statuses.h cl.h message.h package.h config.h
	gcc -c ${CFLAGS} -fPIC -DPIC -o package.o package.c
statuses.o: statuses.c mrim.h statuses.h cl.h message.h package.h config.h
	gcc -c ${CFLAGS} -fPIC -DPIC -o statuses.o statuses.c
cl.o: cl.c mrim.h statuses.h cl.h message.h package.h config.h
	gcc -c ${CFLAGS} -fPIC -DPIC -o cl.o cl.c
message.o: message.c mrim.h statuses.h cl.h message.h package.h config.h
	gcc -c ${CFLAGS} -fPIC -DPIC -o message.o message.c
util.o: util.c util.h mrim.h config.h
	gcc -c ${CFLAGS} -fPIC -DPIC -o util.o util.c
i18n:
	msgfmt ${POPRJ}-ru_RU.po --output-file=${POPRJ}-ru_RU.mo
