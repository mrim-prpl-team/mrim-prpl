#include "mrim.h"
#include "cl.h"
#include "statuses.h"

GList* mrim_status_types(PurpleAccount* account) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	GList *status_list = NULL;
	gint i;
	PurpleStatusType *type;
	for (i = 0; i < MRIM_STATUS_COUNT; i++) {
		type = purple_status_type_new_with_attrs(mrim_statuses[i].primative, mrim_statuses[i].id, _(mrim_statuses[i].title), TRUE, mrim_statuses[i].user_settable, FALSE, "message", _("Message"), purple_value_new(PURPLE_TYPE_STRING), NULL);
		status_list = g_list_append(status_list, type);
	}
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_MOOD, "mood", NULL, FALSE, TRUE, TRUE, PURPLE_MOOD_NAME, _("Mood Name"), purple_value_new( PURPLE_TYPE_STRING ), NULL);
	status_list = g_list_append(status_list, type);
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_TUNE, "tune", NULL, FALSE, TRUE, TRUE, PURPLE_TUNE_ARTIST, _("Tune artist"), purple_value_new( PURPLE_TYPE_STRING ), PURPLE_TUNE_TITLE, _("Tune title"), purple_value_new( PURPLE_TYPE_STRING ),PURPLE_TUNE_ALBUM, _("Tune album"), purple_value_new( PURPLE_TYPE_STRING ), NULL);
	status_list = g_list_append(status_list, type);
	return status_list;
}

PurpleMood *mrim_get_moods(PurpleAccount *account) {
	return moods;
}

void mrim_set_status(PurpleAccount *acct, PurpleStatus *status) {
	MrimData *mrim = acct->gc->proto_data;
	g_return_if_fail(mrim != NULL);
	free_mrim_status(mrim->status);
	mrim->status = make_mrim_status_from_purple(status);
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_CHANGE_STATUS);
	mrim_package_add_UL(pack, mrim->status->id);
	mrim_package_add_LPSA(pack, mrim->status->uri);
	mrim_package_add_LPSW(pack, mrim->status->title);
	mrim_package_add_LPSW(pack, mrim->status->desc);
	mrim_package_add_UL(pack, COM_SUPPORT);
	mrim_package_send(pack, mrim);
	pack = mrim_package_new(mrim->seq++, MRIM_CS_MICROBLOG_POST);
	mrim_package_add_UL(pack, MRIM_BLOG_STATUS_MUSIC);
	mrim_package_add_LPSW(pack, mrim->status->desc);
	mrim_package_send(pack, mrim);
}

char *mrim_status_text(PurpleBuddy *buddy) {
	g_return_val_if_fail(buddy != NULL, NULL);
	if (buddy->proto_data) {
		MrimBuddy *mb = buddy->proto_data;
		if (mb->status) {
			return g_strdup(mb->status->display_str); //TODO: Mem leak
		}
	}
	return NULL;
}

void free_mrim_status(MrimStatus *status) {
	if (status) {
		g_free(status->purple_id);
		g_free(status->purple_tune_artist);
		g_free(status->purple_tune_album);
		g_free(status->purple_tune_title);
		g_free(status->purple_mood);
		g_free(status->uri);
		g_free(status->title);
		g_free(status->desc);
		g_free(status->display_str);
	}
}

MrimStatus *make_mrim_status(guint32 id, gchar *uri, gchar *title, gchar *desc) {
	MrimStatus *status = g_new0(MrimStatus, 1);
	status->id = id;
	status->uri = uri;
	status->title = title;
	status->desc = desc;
	if (uri) {
		guint i;
		for (i = 0; i < MRIM_MOOD_COUNT; i++) {
			if (g_strcmp0(uri, mrim_moods[i].uri) == 0) {
				status->purple_mood = g_strdup(mrim_moods[i].mood);
				status->purple_title = g_strdup(_(mrim_moods[i].title));
				break;
			}
		}
		if (!status->purple_mood) {
			for (i = 0; i < MRIM_STATUS_COUNT; i++) {
				if (g_strcmp0(uri, mrim_statuses[i].uri) == 0) {
					status->purple_id = g_strdup(mrim_statuses[i].id);
					status->purple_title = g_strdup(_(mrim_statuses[i].title));
					break;
				}
			}
		}
	}
	if (!status->purple_id) {
		guint i;
		if (id != STATUS_USER_DEFINED) {
			for (i = 0; i < MRIM_STATUS_COUNT; i++) {
				if ((mrim_statuses[i].code == id) || (mrim_statuses[i].code & id)) {
					status->purple_id = g_strdup(mrim_statuses[i].id);
					status->purple_title = g_strdup(_(mrim_statuses[i].title));
					break;
				}
			}
		}
		if (!status->purple_id) {
			status->purple_id = g_strdup(mrim_statuses[0].id);
		}
	}
	if (title && desc) {
		status->display_str = g_strdup_printf("%s - %s", title, desc);
	} else if (title) {
		status->display_str = g_strdup(title);
	} else if (desc) {
		status->display_str = g_strdup_printf("%s - %s", _(status->purple_title), desc);
	} else {
		status->display_str = g_strdup(_(status->purple_title));
	}
	
	return status;
}

MrimStatus *make_mrim_status_from_purple(PurpleStatus *status) {
	MrimStatus *s = g_new0(MrimStatus, 1);
	const char *id = purple_status_type_get_id(purple_status_get_type(status));
	gint i, status_index = -1;
	if (id) {
		for (i = 0; i < MRIM_STATUS_COUNT; i++) {
			if (g_strcmp0(mrim_statuses[i].id, id) == 0) {
				status_index = i;
				break;
			}
		}
	}
	if (status_index == -1) {
		status_index = 0;
	}
	s->purple_mood = g_strdup(purple_status_get_attr_string(status, PURPLE_MOOD_NAME));
	s->purple_id = g_strdup(mrim_statuses[status_index].id);
	s->purple_tune_artist = g_strdup(purple_status_get_attr_string(status, PURPLE_TUNE_ARTIST));
	s->purple_tune_album = g_strdup(purple_status_get_attr_string(status, PURPLE_TUNE_ALBUM));
	s->purple_tune_title = g_strdup(purple_status_get_attr_string(status, PURPLE_TUNE_TITLE));
	s->title = purple_markup_strip_html(purple_status_get_attr_string(status, "message"));
	if (s->purple_mood) {
		s->id = STATUS_USER_DEFINED;
		s->uri = NULL;
		for (i = 0; i < MRIM_MOOD_COUNT; i++) {
			if (g_strcmp0(s->purple_mood, mrim_moods[i].mood) == 0) {
				s->uri = g_strdup(mrim_moods[i].uri);
				if (!s->title) {
					s->title = g_strdup(_(mrim_moods[i].title));
				}
				break;
			}
		}
		if (!s->uri) {
			s->uri = g_strdup(s->purple_mood);
			if (!s->title) {
				s->title = g_strdup(_(mrim_statuses[status_index].title));
			}
		}
	} else {
		s->id = mrim_statuses[status_index].code;
		s->uri = g_strdup(mrim_statuses[status_index].uri);
		if (s->title) {
			s->id = STATUS_USER_DEFINED;
		} else {
			s->title = g_strdup(_(mrim_statuses[status_index].title));
		}
	}
	if (s->purple_tune_title || s->purple_tune_album || s->purple_tune_artist) {
		gchar *parts[4] = {NULL, NULL, NULL, NULL};
		guint i = 0;
		if (s->purple_tune_artist && s->purple_tune_artist[0]) parts[i++] = s->purple_tune_artist;
		if (s->purple_tune_album && s->purple_tune_album[0]) parts[i++] = s->purple_tune_album;
		if (s->purple_tune_title && s->purple_tune_album[0]) parts[i++] = s->purple_tune_title;
		s->desc = g_strjoinv(" - ", parts);
	} else {
		s->desc = NULL;
	}
	return s;
}


void update_buddy_status(PurpleBuddy *buddy) {
	MrimBuddy *mb = buddy->proto_data;
	if (mb) {
		MrimData *mrim = mb->mrim;	
		purple_prpl_got_user_status(mrim->gc->account, mb->email, mb->status->purple_id, NULL);
		if (mb->status->purple_mood) {
			purple_prpl_got_user_status(mrim->gc->account, mb->email, "mood",
				PURPLE_MOOD_NAME, mb->status->purple_mood,
				PURPLE_MOOD_COMMENT, mb->status->desc,
				NULL);
		} else {
			purple_prpl_got_user_status_deactive(mrim->gc->account, mb->email, "mood");
		}
	}
}

void set_buddy_microblog(MrimData *mrim, PurpleBuddy *buddy, gchar *microblog, guint32 flags) {
	MrimBuddy *mb = buddy->proto_data;
	if (mb) {
		if (flags & MRIM_BLOG_STATUS_UPDATE) {
			g_free(mb->microblog);
			mb->microblog = g_strdup(microblog);
		}
		if (flags & MRIM_BLOG_STATUS_MUSIC) {
			g_free(mb->listening);
			mb->listening = g_strdup(microblog);
		} else {
			if (mrim->micropost_notify) {
				serv_got_im(mrim->gc, mb->email, microblog, PURPLE_MESSAGE_WHISPER, time(NULL));
			}
		}
	}
}

void generate_mood_list() {
	moods = g_new0(PurpleMood, MRIM_MOOD_COUNT + 1);
	guint i;
	for (i = 0; i < MRIM_MOOD_COUNT; i++) {
		moods[i].mood = mrim_moods[i].mood;
		moods[i].description = _(mrim_moods[i].title);
	}
}
