#ifndef MRIM_CONVERT_H
 #define MRIM_CCONVERT_H
 #include "mrim.h"

gchar *mrim_transliterate(const gchar *src_text, const gchar *locale /* Optional */);

// opens url in format http:/foo.bar/%s/%s  where %s/%s - domain/username
void mrim_open_myworld_url(gchar *username, gchar *url);
#endif
