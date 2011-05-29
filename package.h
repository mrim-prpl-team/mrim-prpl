#ifndef MRIM_PACKAGE_H
#define MRIM_PACKAGE_H

#include "mrim.h"

typedef struct {
	mrim_packet_header_t *header;
	gchar *data;
	gsize cur;
	gsize data_size;
} MrimPackage;

MrimPackage *mrim_package_new(guint32 seq, guint32 type);
void mrim_package_free(MrimPackage *pack);

gboolean mrim_package_send(MrimPackage *pack, MrimData *mrim);
MrimPackage *mrim_package_read(MrimData *mrim);

gboolean mrim_package_read_raw(MrimPackage *pack, gpointer buffer, gsize size);
guint32 mrim_package_read_UL(MrimPackage *pack);
gchar *mrim_package_read_LPSA(MrimPackage *pack);
gchar *mrim_package_read_LPSW(MrimPackage *pack);
gchar *mrim_package_read_UIDL(MrimPackage *pack);
gchar *mrim_package_read_LPS(MrimPackage *pack);

void mrim_package_add_raw(MrimPackage *pack, gchar *data, gsize data_size);
void mrim_package_add_UL(MrimPackage *pack, guint32 value);
void mrim_package_add_LPSA(MrimPackage *pack, gchar *string);
void mrim_package_add_LPSW(MrimPackage *pack, gchar *string);
void mrim_package_add_UIDL(MrimPackage *pack, gchar *uidl);

#endif
