#ifndef MRIM_UTIL_H
 #define MRIM_UTIL_H
 #include "mrim.h"

gchar *mrim_transliterate(const gchar *src_text, const gchar *locale /* Optional */);

// opens url in format http:/foo.bar/%s/%s  where %s/%s - domain/username
void mrim_open_myworld_url(gchar *username, gchar *url);

// normalize usernames to canonical names
const char *mrim_normalize(const PurpleAccount *, const char *who);

// converts string(hex-number) to guint32
guint32 atox(gchar *str);

// Returns string value(copy) unless it is NULL or zero-sized (then both NULL).
gchar *mrim_str_non_empty(gchar *str);
#endif
