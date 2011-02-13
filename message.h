#ifndef MESSAGE_H
 #define MESSAGE_H
#include "package.h"

int mrim_send_im(PurpleConnection *gc, const char *to, const char *message, PurpleMessageFlags flags);
gboolean mrim_send_attention(PurpleConnection  *gc, const char *username, guint type);
unsigned int mrim_send_typing(PurpleConnection *gc, const char *name,PurpleTypingState typing);

void mrim_message_offline(PurpleConnection *gc, char* message);
void mrim_read_im(mrim_data *mrim, package *pack);

GList *mrim_chat_info(PurpleConnection *gc);
GHashTable *mrim_chat_info_defaults(PurpleConnection *gc, const char *chat_name);
void mrim_chat_join(PurpleConnection *gc, GHashTable *components);
void mrim_reject_chat(PurpleConnection *gc, GHashTable *components);
char *mrim_get_chat_name(GHashTable *components);
void mrim_chat_invite(PurpleConnection *gc, int id, const char *message, const char *who);
void mrim_chat_leave(PurpleConnection *gc, int id);
void mrim_chat_whisper(PurpleConnection *gc, int id, const char *who, const char *message);
int mrim_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
void mirm_set_chat_topic(PurpleConnection *gc, int id, const char *topic);
//PurpleChat *(* 	find_blist_chat )(PurpleAccount *account, const char *name)
//PurpleRoomlist *(* 	roomlist_get_list )(PurpleConnection *gc)
//void(* 	roomlist_cancel )(PurpleRoomlist *list)
//void(* 	roomlist_expand_category )(PurpleRoomlist *list, PurpleRoomlistRoom *category)
//char *(* 	roomlist_room_serialize )(PurpleRoomlistRoom *room)
//PurpleMood *(* 	get_moods )(PurpleAccount *account)

void mrim_message_status(package *pack);
#endif
