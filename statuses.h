#ifndef MRIM_STATUS_H
#define MRIM_STATUS_H

#include "mrim.h"

typedef struct {
	gchar *purple_id;
	gchar *purple_title;
	gchar *purple_mood;
	gchar *purple_tune_artist;
	gchar *purple_tune_album;
	gchar *purple_tune_title;
	guint32 id;
	gchar *uri;
	gchar *title;
	gchar *desc;
	gchar *display_str;
} MrimStatus;

static struct {
	PurpleStatusPrimitive primative;
	guint32 code;
	gchar* uri;
	gchar* id;
	gchar* title;
	gboolean user_settable;
} mrim_statuses[] = {
	{ PURPLE_STATUS_AVAILABLE, STATUS_ONLINE, "status_1", "status_online", N_("Available"), TRUE },
	{ PURPLE_STATUS_AWAY, STATUS_AWAY, "status_2", "status_away", N_("Away"), TRUE },
	{ PURPLE_STATUS_UNAVAILABLE, STATUS_USER_DEFINED, "status_dnd", "status_unavailable", N_("Unavailable"), TRUE },
	{ PURPLE_STATUS_INVISIBLE, STATUS_FLAG_INVISIBLE, "status_3", "invisible", N_("Invisible"), TRUE },
	{ PURPLE_STATUS_OFFLINE, STATUS_OFFLINE, NULL, "offline", N_("Offline"), TRUE }
};

#define MRIM_STATUS_COUNT ARRAY_SIZE(mrim_statuses)

static struct {
	const char *mood;
	const char *uri;
	const char *title;
} mrim_moods[] = {
	{ "meeting", "status_chat", N_("Ready to chat") },
	{ "sick", "status_4", N_("Sick") },
	{ "plate", "status_6", N_("Plate") },
	{ "restroom", "status_8", N_("Restroom") },
	{ "tongue", "status_14", N_("Tongue") },
	{ "bathing", "status_15", N_("Bathing") },
	{ "console", "status_16", N_("Playing") },
	{ "cigarette", "status_17", N_("Smoking") },
	{ "working", "status_22", N_("Working") },
	{ "sleeping", "status_23", N_("Sleeping") },
	{ "happy", "status_29", N_("Happy") },
	{ "wink", "status_30", N_("Wink") },
	{ "sad", "status_34", N_("Sad") },
	{ "crying", "status_35", N_("Crying") },
	{ "angry", "status_37", N_("Angry") },
	{ "in_love", "status_40", N_("In love") },
	{ "music", "status_53", N_("Music") }
};

#define MRIM_MOOD_COUNT ARRAY_SIZE(mrim_moods)

PurpleMood *moods;

GList* mrim_status_types(PurpleAccount* account);
PurpleMood *mrim_get_moods(PurpleAccount *account);
void mrim_set_status(PurpleAccount *acct, PurpleStatus *status);
char *mrim_status_text(PurpleBuddy *buddy);
void free_mrim_status(MrimStatus *status);
MrimStatus *make_mrim_status(guint32 id, gchar *uri, gchar *title, gchar *desc);
MrimStatus *make_mrim_status_from_purple(PurpleStatus *status);
void update_buddy_status(PurpleBuddy *buddy);
void set_buddy_microblog(MrimData *mrim, PurpleBuddy *buddy, gchar *microblog, guint32 flags);
void generate_mood_list();

#endif
