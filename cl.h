#ifndef MRIM_CL_H
#define MRIM_CL_H

#include "mrim.h"
#include "package.h"
#include "statuses.h"

typedef struct {
	guint32 id;
	guint32 flags;
	gchar *name;
	PurpleGroup *group;
} MrimGroup;

typedef struct {
	MrimData *mrim;
	PurpleBuddy *buddy;
	guint32 id;
	gchar *email;
	gchar *alias;
	guint32 flags;
	guint32 group_id;
	guint32 s_flags;
	gboolean authorized;
	MrimStatus *status;
	gchar *user_agent;
	gchar *microblog;
	gchar **phones;
	gchar *listening;
	guint32 com_support;
} MrimBuddy;

typedef struct {
	PurpleBuddy *buddy;
	PurpleGroup *group;
	gboolean move;
} AddContactInfo;

typedef struct {
	PurpleBuddy *buddy;
} BuddyAddInfo;

typedef struct {
	MrimData *mrim;
	gchar *from;
	guint32 seq;
} MrimAuthData;

typedef struct {
	gchar *title;
	gboolean unicode;
	gboolean skip;
} MrimSearchResultColumn;

typedef struct {
	guint column_count;
	MrimSearchResultColumn *columns;
	guint row_count;
	gchar ***rows;
	guint username_index;
	guint domain_index;
} MrimSearchResult;

#ifdef ENABLE_GTK

typedef struct {
	PurpleBuddy *buddy;
	MrimData *mrim;
	MrimBuddy *mb;
	GtkDialog *dialog;
	GtkTextView *message_text;
	GtkCheckButton *translit;
	GtkLabel *char_counter;
	GtkComboBox *phone;
	gchar *sms_text;
} SmsDialogParams;

#endif

static const gchar *zodiac[] = {
	N_("Aries"),
	N_("Taurus"),
	N_("Gemini"),
	N_("Cancer"),
	N_("Leo"),
	N_("Virgo"),
	N_("Libra"),
	N_("Scorpius"),
	N_("Sagittarius"),
	N_("Capricornus"),
	N_("Aquarius"),
	N_("Pisces")
};

static const gchar *user_info_fields[] = { //For translation
	N_("Nickname"),
	N_("FirstName"),
	N_("LastName"),
	N_("Sex"),
	N_("Birthday"),
	N_("Location"),
	N_("Zodiac"),
	N_("Phone"),
	N_("status_uri"),
	N_("status_title"),
	N_("status_desc"),
	N_("ua_features"),
	N_("Age")
};

void mrim_cl_load(MrimPackage *pack, MrimData *mrim);
void free_mrim_buddy(MrimBuddy *mb);

void mrim_rename_group(PurpleConnection *gc, const char *old_name, PurpleGroup *group, GList *moved_buddies);
void mrim_remove_group(PurpleConnection *gc, PurpleGroup *group);
MrimGroup *new_mrim_group(MrimData *mrim, guint32 id, gchar *name, guint32 flags);
void free_mrim_group(MrimGroup *group);
MrimGroup *get_mrim_group(MrimData *mrim, guint32 id);
MrimGroup *get_mrim_group_by_name(MrimData *mrim, gchar *name);

void mrim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void mrim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void mrim_free_buddy(PurpleBuddy *buddy);
void mrim_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
void mrim_move_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group);

const char *mrim_normalize(const PurpleAccount *account, const char *who);

GList *mrim_user_actions(PurpleBlistNode *node);
void mrim_get_info(PurpleConnection *gc, const char *username);
void mrim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *info, gboolean full);

void mrim_search_ack(MrimData *mrim, gpointer user_data, MrimPackage *pack);

void blist_authorize_menu_item(PurpleBlistNode *node, gpointer userdata);
void blist_edit_phones_menu_item(PurpleBlistNode *node, gpointer userdata);
void blist_gtk_sms_menu_item(PurpleBlistNode *node, gpointer userdata);
void  blist_sms_menu_item(PurpleBlistNode *node, gpointer userdata);
void mrim_url_menu_action(PurpleBlistNode *node, gpointer userdata);

void mrim_authorization_yes(gpointer va_data);
void mrim_authorization_no(gpointer va_data);
void mrim_send_authorize(MrimData *mrim, gchar *email, gchar *message);


void mrim_chat_join(PurpleConnection *gc, GHashTable *components);
void mrim_reject_chat(PurpleConnection *gc, GHashTable *components);
char *mrim_get_chat_name(GHashTable *components);
void mrim_chat_invite(PurpleConnection *gc, int id, const char *message, const char *who);
void mrim_chat_leave(PurpleConnection *gc, int id);
PurpleRoomlist *mrim_roomlist_get_list(PurpleConnection *gc);
void mrim_roomlist_cancel(PurpleRoomlist *list);
void mrim_roomlist_expand_category(PurpleRoomlist *list,	PurpleRoomlistRoom *category);

#endif
