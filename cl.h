#ifndef MRIM_CL_H
 #define MRIM_CL_H
 #include "mrim.h"
 #include "proto.h"
 #include "package.h"
 
 #define MRIM_NO_GROUP 12345


static const gchar *zodiak[]={
	N_("Aries"),
	N_("Taurus"),
	N_("Gemini"),
	N_("Cancer"),
	N_("Leo"),
	N_("Virgo"),
	N_("Libra"),
	N_("Scorpius"),
	N_("Sagittarius"),
	N_("Capricornus"), // 10 in mrim, 9 in massive
	N_("Aquaruis"),
	N_("Pisces")
};

//	Buddy list
void mrim_cl_load(PurpleConnection *gc, mrim_data *mrim, package *pack);
static mrim_buddy *new_mrim_buddy(package *pack);
static void cl_skeep(gchar *mask, package *pack);

//	Groups
static void mrim_add_group(mrim_data *mrim, char *name);
void mrim_rename_group(PurpleConnection *gc, const char *old_name,PurpleGroup *group, GList *moved_buddies);
void mrim_remove_group(PurpleConnection *gc, PurpleGroup *group);
static void mg_add(guint32 flags, gchar *name, guint id, mrim_data *mrim);
PurpleGroup *get_mrim_group_by_id(mrim_data *mrim, guint32 id);
guint32 get_mrim_group_id_by_name(mrim_data *mrim, gchar *name);

//	Buddies
static mrim_buddy *new_mrim_buddy(package *pack);
void mrim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void mrim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void mrim_alias_buddy(PurpleConnection *gc, const char *who, const char *alias);
void mrim_move_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group);
void free_buddy(PurpleBuddy *buddy);
static void free_buddy_proto_data(PurpleBuddy *buddy);

//	Userpics
static void mrim_fetch_avatar(PurpleBuddy *buddy);
static void mrim_avatar_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);

//	Authorization
void mrim_authorization_yes(void *va_data);
void mrim_authorization_no(void *va_data);

//	PQ. The Pending Queue // Pawn-Queen =))
//   Implemented to track pending actions.
void mrim_sms_ack(mrim_data *mrim ,package *pack);
void mrim_add_contact_ack(mrim_data *mrim ,package *pack);
void mrim_modify_contact_ack(mrim_data *mrim ,package *pack);
void mrim_mpop_session(mrim_data *mrim ,package *pack);
void mrim_anketa_info(mrim_data *mrim, package *pack);
void pq_free_element(gpointer data);
void mg_free_element(gpointer data);

static void print_cl_status(guint32 status);

void send_package_authorize(mrim_data *mrim, gchar *to, gchar *who);

void mrim_searchresults_add_buddy(PurpleConnection *gc, GList *row, void *user_data);

gchar *mrim_phones_to_string(gchar **phones);

gboolean mrim_send_sms(gchar *phone, gchar *message, mrim_data *mrim);

void mrim_pkt_modify_buddy(mrim_data *mrim, PurpleBuddy *buddy, guint32 seq);
void mrim_pkt_modify_group(mrim_data *mrim, guint32 group_id, gchar *group_name, guint32 flags);
void mrim_pkt_add_group(mrim_data *mrim, gchar *group_name, guint32 seq);
#endif
