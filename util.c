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
	return string_is_match(email, "([[:alnum:]\\_]+[[:alnum:]\\-\\.\\_]+)@(mail.ru|list.ru|inbox.ru|bk.ru|corp.mail.ru)");
}

gboolean is_valid_chat(gchar *chat) {
	return string_is_match(chat, "([0-9])+@(chat.agent)");
}

gboolean is_valid_phone(gchar *phone) {
	return string_is_match(phone, "([+]{0,1}[0-9]{10,12})");
}

gchar *mrim_get_ua_alias(gchar *ua) {
	gchar *client_id = NULL;
	gchar *client_version = NULL;
	gchar *client_build = NULL;
	gchar *client_ui = NULL;
	gchar *client_title = NULL;
	gchar *alias;
	GMatchInfo *match_info;
	GRegex *regex = g_regex_new("([A-Za-z]*)=\"([^\"]*)\"", 0, 0, NULL);
	g_regex_match(regex, ua, 0, &match_info);
	while (g_match_info_matches(match_info)) {
		gchar *key = g_match_info_fetch(match_info, 1);
		gchar *value = g_match_info_fetch(match_info, 2);
		if (g_strcmp0(key, "client") == 0) {
			client_id = g_strdup(value);
		} else if (g_strcmp0(key, "version") == 0) {
			client_version = g_strdup(value);
		} else if (g_strcmp0(key, "build") == 0) {
			client_build = g_strdup(value);
		} else if (g_strcmp0(key, "ui") == 0) {
			client_ui = g_strdup(value);
		}
		g_free(key);
		g_free(value);
		g_match_info_next(match_info, NULL);
	}
	g_match_info_free(match_info);
	g_regex_unref(regex);
	if (client_id) {
		guint i;
		client_title = client_id;
		for (i = 0; i < ARRAY_SIZE(ua_titles); i++) {
			if (g_strcmp0(client_id, ua_titles[i].id) == 0) {
				client_title = _(ua_titles[i].title);
			}
		}
	}
	if (client_id && client_version && client_build && client_ui) {
		alias = g_strdup_printf(_("%s with %s (version %s, build %s)"),
			client_ui, client_title, client_version, client_build);
	} else if (client_id && client_version && client_ui) {
		alias = g_strdup_printf(_("%s with %s (version %s)"), client_ui, client_title, client_version);
	} else if (client_id && client_ui) {
		alias = g_strdup_printf(_("%s with %s"), client_ui, client_title);
	} else if (client_id && client_version && client_build) {
		alias = g_strdup_printf(_("%s (version %s, build %s)"), client_title, client_version, client_build);
	} else if (client_id && client_version) {
		alias = g_strdup_printf(_("%s (version %s)"), client_title, client_version);
	} else if (client_id) {
		alias = g_strdup(client_title);
	} else {
		alias = g_strdup(ua);
	}
	g_free(client_id);
	g_free(client_version);
	g_free(client_build);
	g_free(client_ui);
	return alias;
}
