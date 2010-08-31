#ifndef MESSAGE_H
 #define MESSAGE_H
#include "package.h"

int mrim_send_im(PurpleConnection *gc, const char *to, const char *message, PurpleMessageFlags flags);
gboolean mrim_send_attention(PurpleConnection  *gc, const char *username, guint type);
unsigned int mrim_send_typing(PurpleConnection *gc, const char *name,PurpleTypingState typing);

void mrim_message_offline(PurpleConnection *gc, char* message);
void mrim_read_im(mrim_data *mrim, package *pack);

void mrim_message_status(mrim_data *mrim, package *pack);
#endif
