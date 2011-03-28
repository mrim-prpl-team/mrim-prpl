#ifndef PACKAGE_H
 #define PACKAGE_H
 #include "proto.h"
 #include "mrim.h"
 #include <stdarg.h>           /* для va_ макросов */
 #include <string.h> // g_memmove
 #include <unistd.h> // функции Read/Write

/* manual: http://www.unixwiz.net/techtips/gnu-c-attributes.html */
#ifndef __GNUC__
#  define  __attribute__(x)  /*NOTHING*/
#endif

#define PACK_MAX_LEN 65536
#define CHUNK 16384

#ifdef WIN32
	#define RECV_FLAGS 0
#else
	#define RECV_FLAGS MSG_WAITALL
#endif

typedef struct
{
	mrim_packet_header_t *header;
	char *buf;// указатель на начало буфера
	char *cur;// указатель на ещё не считанные данные
//	char *end;// указатель на последний элемент буфера
	u_int len;// длина буффера
}package;

package *new_package(guint32 seq,guint32 type);
void add_ul(guint32 ul, package *pack);
void add_LPS(gchar *string, package *pack);
void add_raw(char *new_data, int len, package *pack);
void add_RTF(gchar *string, package *pack);
void add_base64(package *pack, gboolean gziped, gchar *fmt, ...)  __attribute__((format(printf,3,4)));

gboolean send_package(package *pack, mrim_data *mrim);

package *read_package(mrim_data *mrim);
guint32 read_UL(package *pack);
gchar *read_rawLPS(package *pack);
gchar *read_LPS(package *pack);
gchar *read_UTF16LE(package *pack);
gchar *read_Z(package *pack);
void read_base64(package *pack, gboolean gziped, gchar *fmt, ...)  __attribute__((format(printf,3,4)));

void free_package(package *pack);
#endif
