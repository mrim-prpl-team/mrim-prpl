#ifndef MRIM_MESSAGE_H
#define MRIM_MESSAGE_H

#include "mrim.h"

int mrim_send_im(PurpleConnection *gc, const char *to, const char *message, PurpleMessageFlags flags);
unsigned int mrim_send_typing(PurpleConnection *gc, const char *name,PurpleTypingState typing);
gboolean mrim_send_attention(PurpleConnection  *gc, const char *username, guint type);
void mrim_receive_im(MrimData *mrim, MrimPackage *pack);
void mrim_receive_offline_message(MrimData *mrim, gchar *message);
void mrim_send_sms(MrimData *mrim, gchar *phone, gchar *message);

#endif
