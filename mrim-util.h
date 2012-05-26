#ifndef MRIM_UTIL_H
#define MRIM_UTIL_H

#include "mrim.h"

time_t mrim_str_to_time(const gchar* str);
gboolean is_valid_email(gchar *email);
gboolean is_myworld_able(gchar *email);
gboolean is_valid_chat(gchar *chat);
gboolean is_valid_phone(gchar *phone);
gchar *mrim_get_ua_alias(MrimData *mrim, gchar *ua);

//gchar *mrim_msg_to_rtf(gchar *message);
gchar *mrim_message_unpack_rtf_part(const gchar *rtf_message);
gchar *mrim_message_from_rtf(const gchar *rtf_message);

int get_chat_id(const char *chatname);
gchar *md5sum(gchar *str);

gchar *transliterate_text(gchar *text);

#endif
