#ifndef MRIM_UTIL_H
 #define MRIM_UTIL_H
 #include "mrim.h"

gchar *mrim_transliterate(const gchar *src_text, const gchar *locale /* Optional */);
// converts string(hex-number) to guint32
guint32 atox(gchar *str);
time_t mrim_str_to_time(const gchar* str);


// Returns string value(copy) unless it is NULL or zero-sized (then both NULL).
gchar *mrim_str_non_empty(gchar *str);
// normalize usernames to canonical names
void clean_string(gchar *email);
gboolean string_is_match(gchar *string, gchar *pattern);
gchar *clear_phone(gchar *phone);

const char *mrim_normalize(const PurpleAccount *, const char *who);
gboolean is_valid_email(gchar *email);
gboolean is_valid_phone(gchar *phone);
gboolean is_valid_chat(gchar *chat);
#define is_valid_buddy_name(name) (is_valid_phone(name) || is_valid_email(name))

// opens url in format http:/foo.bar/%s/%s  where %s/%s - domain/username
void mrim_open_myworld_url(gchar *username, gchar *url);
#endif
