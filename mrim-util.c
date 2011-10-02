#include "mrim.h"
#include "util.h"

time_t mrim_str_to_time(const gchar* str) {
	int year=0, month=0, day=0, hour=0, min=0, sec=0;
	gchar month_str[4];
	int ret = sscanf(str, "%*03s, %i %03s %i %i:%i:%i", &day, month_str, &year, &hour, &min, &sec);
	if(g_strcmp0(month_str, "Jan") == 0) month=1;
	else if(g_strcmp0(month_str, "Feb") == 0) month=2;
	else if(g_strcmp0(month_str, "Mar") == 0) month=3;
	else if(g_strcmp0(month_str, "Apr") == 0) month=4;
	else if(g_strcmp0(month_str, "May") == 0) month=5;
	else if(g_strcmp0(month_str, "Jun") == 0) month=6;
	else if(g_strcmp0(month_str, "Jul") == 0) month=7;
	else if(g_strcmp0(month_str, "Aug") == 0) month=8;
	else if(g_strcmp0(month_str, "Sep") == 0) month=9;
	else if(g_strcmp0(month_str, "Oct") == 0) month=10;
	else if(g_strcmp0(month_str, "Nov") == 0) month=11;
	else if(g_strcmp0(month_str, "Dec") == 0) month=12;
	return purple_time_build(year, month, day, hour, min, sec);
}

gboolean string_is_match(gchar *string, gchar *pattern) {
	g_return_val_if_fail(string, FALSE);
	g_return_val_if_fail(pattern, FALSE);
	GRegex *regex;
	gboolean res;
	GMatchInfo *match_info;

    regex = g_regex_new (pattern, G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);
    res = g_regex_match (regex, string, 0, &match_info);
	// TODO Mem free.
    g_match_info_free(match_info);
    g_regex_unref(regex);
    return res;
}

gboolean is_valid_email(gchar *email) {
	return string_is_match(email, "([0-9a-zA-Z\\_]+[0-9a-zA-Z\\-\\.\\_]*)@(uin.icq|aol.com|mail.ru|list.ru|inbox.ru|bk.ru|corp.mail.ru)");
}

gboolean is_valid_chat(gchar *chat) {
	return string_is_match(chat, "([0-9])+@(chat.agent)");
}

gboolean is_valid_phone(gchar *phone) {
	return string_is_match(phone, "([+]{0,1}[0-9]{10,12})");
}

gchar *mrim_get_ua_alias(MrimData *mrim, gchar *ua) {
	if (purple_account_get_bool(mrim->gc->account, "debug_mode", FALSE)) {
		return g_strdup(ua);
	}
	gchar *ua_received = g_strdup(ua);
	gchar *client_id = NULL;
	gchar *client_version = NULL;
	gchar *client_build = NULL;
	gchar *client_ui = NULL;
	gchar *client_title = NULL;
	gchar *client_name = NULL;
	gchar *client_protocol = NULL;
	gchar *alias;
	GMatchInfo *match_info;
	GRegex *regex = g_regex_new("([A-Za-z]*)=\"([^\"]*?)\"", 0, 0, NULL);
	g_regex_match(regex, ua, 0, &match_info);
	guint counter = 0;
	while (g_match_info_matches(match_info)) {
		gchar *key = g_match_info_fetch(match_info, 1);
		gchar *value = g_match_info_fetch(match_info, 2);
		if (g_strcmp0(key, "client") == 0) {
			client_id = g_strdup(value);
		} else if (g_strcmp0(key, "version") == 0) {
			client_version = g_strdup(value);
			counter++;
		} else if (g_strcmp0(key, "build") == 0) {
			client_build = g_strdup(value);
			counter++;
		} else if (g_strcmp0(key, "ui") == 0) {
			client_ui = g_strdup(value);
		} else if (g_strcmp0(key, "name") == 0) {
			client_name = g_strdup(_(value));
		} else if (g_strcmp0(key, "title") == 0) {
			client_title = g_strdup(_(value));
		} else if (g_strcmp0(key, "protocol") == 0) {
			client_protocol = g_strdup(value);
			counter++;
		}
		g_free(key);
		g_free(value);
		g_match_info_next(match_info, NULL);
	}
	g_match_info_free(match_info);
	g_regex_unref(regex);
	if (client_name && client_id && (g_strcmp0(client_id, MRIM_MAGENT_ID) == 0) ) {
		gchar *new_title = g_strdup(client_name);
		g_free(client_title);
		client_title = new_title;
	} else if (client_id) {
		guint i;
		gchar *new_title = NULL;
		for (i = 0; i < ARRAY_SIZE(ua_titles); i++) {
			if (g_strcmp0(client_id, ua_titles[i].id) == 0) {
				new_title = g_strdup(_(ua_titles[i].title));
				g_free(client_title);
				client_title = new_title;
				break; // Think we should stop it.
			}
		}
		if (!client_title) {
			new_title = g_strdup(_(ua_titles[i].title));
			g_free(client_title);
			client_title = new_title;
		}
	} else {
		gchar *new_title = g_strdup(ua_received);
		g_free(client_title);
		client_title = new_title;
	}
	gchar *info = NULL;
	if (counter) {
		gchar **infos = g_new0(gchar*, counter + 1);
		guint i = 0;
		if (client_version) {
			infos[i] = g_strdup_printf(_("version %s"), client_version);
			i++;
		}
		if (client_build) {
			infos[i] = g_strdup_printf(_("build %s"), client_build);
			i++;
		}
		if (client_protocol) {
			infos[i] = g_strdup_printf(_("protocol version %s"), client_protocol);
			i++;
		}
		info = g_strjoinv(", ", infos);
		g_strfreev(infos);
	}
	gchar *name = NULL;
	if (client_ui) {
		name = g_strdup_printf(_("%s with %s"), client_ui, client_title);
	} else {
		name = g_strdup(client_title);
	}
	if (info) {
		gchar *new_name = g_strdup_printf("%s (%s)", name, info);
		g_free(name);
		name = new_name;
	}
	alias = g_strdup(name);
	g_free(name);
	g_free(info);
	g_free(client_id);
	g_free(client_version);
	g_free(client_build);
	g_free(client_name);
	g_free(client_title);
	g_free(client_protocol);
	g_free(client_ui);
	g_free(ua_received);
	return alias;
}

int get_chat_id(const char *chatname) {
	return atoi(chatname);
	// or just  g_str_hash() from <glib.h>
}

gchar *md5sum(gchar *str) {
	PurpleCipher *cipher;
	PurpleCipherContext *context;
	int len = strlen(str);
	gchar *result = g_new0(gchar, 17); // md5 - это 16 символов

	cipher = purple_ciphers_find_cipher("md5");
	context = purple_cipher_context_new(cipher, NULL);
	purple_cipher_context_append(context, str, len);
	purple_cipher_context_digest(context, 16, result, NULL);
	purple_cipher_context_destroy(context);

	purple_debug_info("mrim-prpl", "[%s] hash: '%s'\n", __func__, result);
	return result;
}

gchar *transliterate_text(gchar *text) {
	gchar *new_text = g_strdup(text);
	gchar *table = _("translit-table");
	if (g_strcmp0(table, "translit-table") == 0) { // Для текущей локали не задана таблица транслитерации
		gchar **rules = g_strsplit(table, ",", 0);
		gchar **rule = rules;
		while (*rule) {
			gchar **r = g_strsplit(*rule, "=", 2);
			gchar **parts = g_strsplit(new_text, r[0], 0);
			g_free(new_text);
			new_text = g_strjoinv(r[1], parts);
			g_strfreev(parts);
			rule++;
		}
		g_strfreev(rules);
	}
	return new_text;
}
