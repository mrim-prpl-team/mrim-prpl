#ifndef MRIM_UTIL_H
#define MRIM_UTIL_H

time_t mrim_str_to_time(const gchar* str);
gboolean is_valid_email(gchar *email);
gboolean is_valid_chat(gchar *chat);
gboolean is_valid_phone(gchar *phone);
gchar *mrim_get_ua_alias(gchar *ua);

int get_chat_id(const char *chatname);

#endif
