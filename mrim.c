/**
 * 
 * Copyright (C) 2010, Антонов Николай (Antonov Nikolay) aka Ostin <antoa@mail.ru> 
 *  
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#define PURPLE_PLUGINS

#include "mrim.h"
#include "package.h"
#include "cl.h"
#include "message.h"
#include "mrim-util.h"
#include "filetransfer.h"

// TODO rewrite
void clean_string(gchar *str)
{
	purple_debug_info("mrim","[%s] %s\n",__func__, str);
	g_return_if_fail(str);
	str = g_strstrip(str); // no memory leaks
	/*while (*str)
	{
		*str = g_ascii_tolower(*str);
		str++;
	}*/
	purple_debug_info("mrim","[%s] %s\n",__func__, str);
}

gboolean ua_matches(const gchar *string, const gchar *pattern, ua_data *result)
{
	g_return_val_if_fail(string, FALSE);
	g_return_val_if_fail(pattern, FALSE);
	GRegex *regex;
	gboolean res;
	GMatchInfo *match_info;

	regex	= g_regex_new (pattern, G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);
	res	= g_regex_match (regex, string, 0, &match_info);
	if(res)
	{
		purple_debug_info("mrim", "regex match! (%s):\n", pattern);
		purple_debug_info("mrim", "ver: %s\n", g_match_info_fetch(match_info,2));
		purple_debug_info("mrim", "bld: %s\n", g_match_info_fetch(match_info,3));
		purple_debug_info("mrim", "mre: %s\n", g_match_info_fetch(match_info,4));
		result->version	= g_match_info_fetch(match_info,2);
		result->build		= g_match_info_fetch(match_info,3);
		result->more		= g_match_info_fetch(match_info,4);
		purple_debug_info("mrim", "regex complete.\n");
	} else
	{
		purple_debug_info("mrim", "No match!\n");
	}
	// TODO Mem free.
	g_match_info_free(match_info);
	g_regex_unref(regex);
	return res;
}

gboolean string_is_match(gchar *string, gchar *pattern)
{
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

gboolean is_valid_email(gchar *email)
{
	return string_is_match(email, "([[:alnum:]\\_]+[[:alnum:]\\-\\.\\_]+)@(mail.ru|list.ru|inbox.ru|bk.ru|corp.mail.ru)");
}

gboolean is_valid_chat(gchar *chat)
{
	return string_is_match(chat, "([0-9])+@(chat.agent)");
}

gboolean is_valid_phone(gchar *phone)
{
	return string_is_match(phone, "([+]{0,1}[0-9]{10,12})");
}

gchar *clear_phone(gchar *original_phone)
{
	// TODO REWRITE
	purple_debug_info("mrim","[%s] <%s>\n",__func__, original_phone);
	if (original_phone == NULL)
		return NULL;

	gchar *phone = g_strstrip(g_strdup(original_phone)); // TODO memleaks

	if (*phone == '+')	// skip "+"
	{
		phone++;
	}
	else if (*phone == '8')
		*phone = '7';
	// keep digits only
	int j=0;
	gchar *correct_phone = g_new0(gchar, 13);
	while(*phone && j<12)
	{
		if ((*phone <= '9') && (*phone >= '0'))
			correct_phone[j++] = *phone;
		else
			if ( ! ((*phone == ' ') || (*phone == '-')) )
			{
				g_free(correct_phone);
				return NULL;
			}
		phone++;
	}
#ifdef DEBUG
	purple_debug_info("mrim","[%s] original=<%s>, correct=<%s>\n",__func__, original_phone, correct_phone );
#endif
	//if (is_valid_phone(correct_phone))
		return correct_phone;

	g_free(correct_phone);
	return NULL;
}

PurpleBuddy *mrim_phone_get_parent_buddy(mrim_data *mrim, gchar *phone)
{
	PurpleBuddy *result = NULL;
	GSList *list, *head;

	head = list = purple_find_buddies(mrim->account, NULL); // get all our buddies
	if (!list)
		return NULL;

	for (; list; list = list->next)
	{
		PurpleBuddy *buddy = (PurpleBuddy *) (list->data);
		purple_debug_info("mrim", "[%s] %s\n", __func__, buddy->name);
		mrim_buddy *mb = purple_buddy_get_protocol_data(buddy);
		if (mb && mb->phones && mb->phones[0])
			for (int i = 0; (i < 3) && (mb->phones[i]); i++)
			{
				purple_debug_info("mrim", "[%s]## %s\n", __func__, mb->phones[i]);
				if (strcmp(mb->phones[i], phone) == 0)
				{
					result = buddy;
					goto exit;
				}
			}
	}
exit:
	g_slist_free(list);
	purple_debug_info("mrim", "[%s]result:%s\n", __func__, (result)?result->name:"NotFound");
	return result;
}



/******************************************
 *             USER INFO
 ******************************************/
static void mrim_get_info(PurpleConnection *gc, const char *username)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(username);
	g_return_if_fail(gc);

	mrim_data *mrim = gc->proto_data;

	purple_debug_info("mrim", "Fetching %s's user info for %s\n", username, gc->account->username);

	if (gc->state != PURPLE_CONNECTED)
	{
		char *msg = g_strdup_printf(_("%s is offline."), gc->account->username);
		purple_notify_error(gc, _("UserInfo"), _("UserInfo is not available."), msg);
		g_free(msg);
	}
	else if (!is_valid_email((gchar*)username))
	{
		PurpleNotifyUserInfo *info = purple_notify_user_info_new();
		purple_notify_user_info_add_pair(info, _("UserInfo is not available for conferences and phones."), "");
		purple_notify_userinfo(gc, username, info, NULL, NULL);
	}
	else
	{
	    gchar** split = g_strsplit(username,"@",2);// split into two parts maximum
		gchar *user = split[0];
		gchar *domain = split[1];

		mrim_pq *mpq = g_new0(mrim_pq ,1);
		mpq->seq = mrim->seq;
		mpq->type = ANKETA_INFO;
		mpq->anketa_info.username = g_strdup(username);
		g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

		package *pack = new_package(mrim->seq, MRIM_CS_WP_REQUEST);
		add_ul(MRIM_CS_WP_REQUEST_PARAM_USER, pack);
		add_LPS(user, pack);
		add_ul(MRIM_CS_WP_REQUEST_PARAM_DOMAIN, pack);
		add_LPS(domain, pack);
		send_package(pack, mrim);

		g_strfreev(split);
	}
}

gchar* mrim_get_ua_alias(const gchar* ua)
{
	purple_debug_info("mrim", "[%s] Gonna parse UA %s.\n", __func__, ua);
	if (ua)
	{
		gchar *alias = ua;
		ua_data *ua_details = g_new0(ua_data, 1);
		ua_details->version	= g_strdup("");
		ua_details->build		= g_strdup("");
		ua_details->more		= g_strdup("");
		guint j = 0;
		while (ua_aliases[j].id)
		{
			purple_debug_info("mrim", "checking for %s.\n", ua_aliases[j].pattern);
			if (ua_matches(ua, ua_aliases[j].pattern, ua_details))
			{
				purple_debug_info("mrim", "UA %s match %s\n", ua, ua_aliases[j].pattern);
				alias = g_strdup_printf(_(ua_aliases[j].alias), ua_details->version, ua_details->build, ua_details->more);
				break;
			} else
				j++;
		}
		return alias;
	} else
	{
		return ua;
	}
}

/******************************************
 *             PRPL ACTION
 ******************************************/
/* Action List of the protocol (Accounts -> mailru -> this list here) */
static void mrim_account_action(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);

	mrim_pq *mpq = g_new0(mrim_pq ,1);
	mpq->seq = mrim->seq;
	mpq->type = OPEN_URL;
	mpq->open_url.url = g_strdup_printf(links[GPOINTER_TO_UINT(action->user_data)], mrim->username);
	purple_debug_info("mrim","[%s] %s\n", __func__, mpq->open_url.url);
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	package *pack = new_package(mpq->seq, MRIM_CS_GET_MPOP_SESSION);
	send_package(pack, mrim);
}

static void mrim_search_action(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	purple_debug_info("mrim","[%s]\n",__func__);

	// REQUEST
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("text_box_nickname",_("Nick"),"",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_first_name",_("Name"),"",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_surname",_("LastName"),"",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_choice_new("radio_button_gender", _("Gender"), 0);
	purple_request_field_choice_add(field, _("No matter")); // TODO maybe, purple_request_field_list* ?
	purple_request_field_choice_add(field, _("Male"));
	purple_request_field_choice_add(field, _("Female"));
	purple_request_field_group_add_field(group, field);
	/* country */
	/* region */
	/* city */
	/* birthday */
	/* zodiak */
	field = purple_request_field_string_new("text_box_age_from",_("Age from"),"",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_age_to",_("Age up to"),"",FALSE);
	purple_request_field_group_add_field(group, field);
	/* webkam */
	/* gotov poboltat' */
	field = purple_request_field_bool_new("check_box_online",_("Online"),FALSE);
	purple_request_field_group_add_field(group, field);



	purple_request_fields(mrim->gc, _("Buddies search"), NULL, NULL,  fields,
			_("_Search!"), G_CALLBACK(blist_search),
			_("_Cancel!"), NULL,
			mrim->account, mrim->username, NULL, mrim->gc );

}

// It is url open actions
static void mrim_easy_action(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);

	mrim_open_myworld_url(mrim->username, links[GPOINTER_TO_UINT(action->user_data)]);
}

static GList *mrim_prpl_actions(PurplePlugin *plugin, gpointer context)
{	// TODO
	purple_debug_info("mrim","[%s]\n",__func__);
	PurplePluginAction *action = NULL;
	GList *actions = NULL;

	action = purple_plugin_action_new(_("[web] Set User Info"), mrim_account_action);
	action->user_data = GUINT_TO_POINTER(MY_PROFILE);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("[web] Set User avatar"), mrim_account_action);
	action->user_data = GUINT_TO_POINTER(MY_AVATAR);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("Search for buddies"), mrim_search_action);
	action->user_data = NULL;
	actions = g_list_append(actions, action);

	actions = g_list_append(actions, NULL); // separator
	// DO NOT REMOVE THIS CODE. WE *MUST* PROVIDE THIS LINKS DUE TO LICENSE AGREEME
	action = purple_plugin_action_new(_("[web] MY_WORLD"), mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_WORLD);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("[web] MY_PHOTO"), mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_PHOTO);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("[web] MY_VIDEO"), mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_VIDEO);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("[web] MY_BLOG"), mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_BLOG);
	actions = g_list_append(actions, action);
	return actions;
}


/******************************************
 *             TOOLTIP
 ******************************************/
static void mrim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *info, gboolean full) 
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(buddy);
	PurpleAccount *account = purple_buddy_get_account(buddy);
	g_return_if_fail(account);
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc)
	{
		/* they're logged in */
		if (buddy->alias)
			purple_notify_user_info_add_pair(info, _("Name"), buddy->alias);

		PurplePresence *presence = purple_buddy_get_presence(buddy);
		PurpleStatus *status = purple_presence_get_active_status(presence);
		gchar *presence_name = purple_status_get_name(status);
		if (!presence_name)
			presence_name = g_strdup("Undefined");
		mrim_buddy *mb = buddy->proto_data;
		if (mb)
		{
			// Presence info:
			if (purple_status_type_get_primitive(purple_status_get_type(status)) != PURPLE_STATUS_OFFLINE) {
				purple_notify_user_info_add_pair(info, _("Status"), g_strdup(mb->status.display_string));
			}
			
			//Microblog
			if (mb->microblog) {
				purple_notify_user_info_add_pair(info, _("Microblog"), g_strdup(mb->microblog));
			}
			
			// Phones info:
			if (mb->phones && mb->phones[0])
				purple_notify_user_info_add_pair(info, _("Phone numbers"), mrim_phones_to_string(mb->phones));
			if (mb->user_agent)
			{
				purple_notify_user_info_add_pair(info, _("User agent"), _(mrim_get_ua_alias(mb->user_agent)) );
			} else if (is_valid_email(mb->addr))
			{
				if (purple_status_type_get_primitive(purple_status_get_type(status)) != PURPLE_STATUS_OFFLINE) {
					purple_notify_user_info_add_pair(info, _("User agent"), _("Hidden") );
				}
			}
		}
		
		if (full)
		{
			const char *user_info = purple_account_get_user_info(account);
			if (user_info)
				purple_notify_user_info_add_pair(info, _("Contact details"), user_info);
		}
	} 
	else
		purple_notify_user_info_add_pair(info, _("Contact details"), _("Offline"));
}




/******************************************
 *             USER ACTION
 ******************************************/
void blist_search(PurpleConnection *gc, PurpleRequestFields *fields)
{
	g_return_if_fail(gc);
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = SEARCH;
	mpq->seq = mrim->seq;
	// Adding mpq to PQ only if we send it.
	package *pack = new_package(mpq->seq, MRIM_CS_WP_REQUEST);

	const char *const_string = NULL;
	gchar *tmp = NULL;
	PurpleRequestField *TextBoxField = NULL;
	PurpleRequestField *RadioBoxField = NULL;
	PurpleRequestField *CheckBoxField = NULL;
	//TextBoxField = purple_request_field_g
	const_string = purple_request_fields_get_string(fields, "text_box_nickname");
	tmp = g_strstrip(g_strdup(const_string));
	if ( (tmp != NULL) && strcmp(tmp,"") )
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_NICKNAME, pack);
		add_LPS(tmp, pack);
	}

	const_string = purple_request_fields_get_string(fields, "text_box_first_name");
	tmp = g_strstrip(g_strdup(const_string));
	if ( (tmp != NULL) && strcmp(tmp,"") )
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_FIRSTNAME, pack);
		add_LPS(tmp, pack);
	}

	const_string = purple_request_fields_get_string(fields, "text_box_surname");
	tmp = g_strstrip(g_strdup(const_string));
	if ( (tmp != NULL) && strcmp(tmp,"") )
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_LASTNAME, pack);
		add_LPS(tmp, pack);
	}

	RadioBoxField = purple_request_fields_get_field(fields, "radio_button_gender");
	int index  = RadioBoxField->u.choice.value;
	if (index)
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_SEX, pack);
		add_LPS(  (index == 1)?"1":"2",  pack);
	}
	/* country 	MRIM_CS_WP_REQUEST_PARAM_COUNTRY_ID */
	/* region */
	/* city MRIM_CS_WP_REQUEST_PARAM_CITY_ID */
	/* birthday */
	/* zodiak MRIM_CS_WP_REQUEST_PARAM_ZODIAC */
	const_string = purple_request_fields_get_string(fields, "text_box_age_from");
	tmp = g_strstrip(g_strdup(const_string));
	if ( (tmp != NULL) && strcmp(tmp,"") )
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_DATE1, pack);
		add_LPS(tmp, pack);
	}
	const_string = purple_request_fields_get_string(fields, "text_box_age_to");
	tmp = g_strstrip(g_strdup(const_string));
	if ( (tmp != NULL) && strcmp(tmp,"") )
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_DATE2, pack);
		add_LPS(tmp, pack);
	}
	/* webkam */
	/* gotov poboltat' */
	CheckBoxField = purple_request_fields_get_field(fields, "check_box_online");
	if (CheckBoxField->u.boolean.value)
	{
		add_ul(MRIM_CS_WP_REQUEST_PARAM_ONLINE, pack);
		add_LPS("1", pack);
	}

	purple_debug_info("mrim", "[%s]pack->len==%u\n", __func__, pack->len);
	if (pack->len > 0)
	{
		send_package(pack, mrim);
		g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq ), mpq);
	}
	//else
	{
	//	free_package(pack);
		// g_free(mpq);
	}
}

void blist_send_sms(PurpleConnection *gc, PurpleRequestFields *fields)
{
	g_return_if_fail(gc);
	PurpleRequestField *RadioBoxField = purple_request_fields_get_field(fields, "combobox");
	int index  = RadioBoxField->u.choice.value;
	GList *list = RadioBoxField->u.choice.labels;
	while (index-- && list)
		list = list->next;

	const char *message = purple_request_fields_get_string(fields, "message_box");
	mrim_send_sms(list->data, message, gc->proto_data);
}

static void  blist_sms_menu_item(PurpleBlistNode *node, gpointer userdata)
{
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	mrim_data *mrim = userdata;
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(mrim != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	// REQUEST
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_choice_new("combobox", _("Choose phone number"), 0);
	purple_request_field_choice_add(field, mb->phones[0]); // TODO may be purple_request_field_list* ?
	purple_request_field_choice_add(field, mb->phones[1]);
	purple_request_field_choice_add(field, mb->phones[2]);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("message_box",_("SMS message text"),"",TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(mrim->gc, _("Send SMS"), NULL, _("SMS message should contain not\nmore than 135 symbols in latin\nor 35 in cyrillic."),  fields,
			_("_Send"), G_CALLBACK(blist_send_sms),
			_("_Cancel!"), NULL,
			mrim->account, buddy->name, NULL, mrim->gc );
}

void update_sms_char_counter(GObject *object, sms_dialog_params *params) {
	gchar *original_text, *new_text;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(params->message_text);
	{
		GtkTextIter start, end;
		gtk_text_buffer_get_start_iter(buffer, &start);
		gtk_text_buffer_get_end_iter(buffer, &end);
		original_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	}
	if (gtk_toggle_button_get_active(params->translit)) {
		/* TODO: транслитерация сообщения */
		new_text = g_strdup(original_text); //new_text должен указывать на транслитерированный текст. original_text должен быть освобождён
	} else {
		new_text = g_strdup(original_text);
	}
	FREE(original_text);
	g_free(params->sms_text);
	params->sms_text = new_text;
	size_t count = g_utf8_strlen(new_text, -1);
	gchar buf[64];
	g_sprintf(&buf, _("Symbols: %d"), count);
	gtk_label_set_text(params->char_counter, buf);
}

void sms_dialog_response(GtkDialog *dialog, gint response_id, sms_dialog_params *params) {
	switch (response_id) {
		case GTK_RESPONSE_ACCEPT:
			{
				mrim_buddy *mb = params->mb;
				mrim_data *mrim = params->mrim;
				gchar *text = params->sms_text;
				gint phone_index = gtk_combo_box_get_active(params->phone);
				if (phone_index > -1) {
					gchar *phone = mb->phones[phone_index];
					mrim_send_sms(phone, text, mrim);
				}
				break;
			}
		case GTK_RESPONSE_REJECT:
			break;
	}
	gtk_widget_destroy(dialog);
}

void sms_dialog_destroy(GtkDialog *dialog, sms_dialog_params *params) {
	g_free(params->sms_text);
	g_free(params);
}

static void  blist_edit_phones_menu_item(PurpleBlistNode *node, gpointer userdata);

void sms_dialog_edit_phones(GtkButton *button, sms_dialog_params *params) {
	blist_edit_phones_menu_item(params->buddy, params->mrim);
	gtk_combo_box_remove_text(params->phone, 2);
	gtk_combo_box_remove_text(params->phone, 1);
	gtk_combo_box_remove_text(params->phone, 0);
	gtk_combo_box_append_text(params->phone, params->mb->phones[0]);
	gtk_combo_box_append_text(params->phone, params->mb->phones[1]);
	gtk_combo_box_append_text(params->phone, params->mb->phones[2]);
	gtk_combo_box_set_active(params->phone, 0);
}

void blist_sms_menu_item_gtk(PurpleBlistNode *node, gpointer userdata) {
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	mrim_data *mrim = userdata;
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(mrim != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	/* Диалог */
	GtkDialog *dialog = gtk_dialog_new_with_buttons(_("Send SMS"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_window_set_default_size(dialog, 320, 240);
	GtkWidget *content_area = gtk_dialog_get_content_area(dialog);
	GtkWidget *hbox;
	//gtk_container_set_border_width(content_area, 8); // Не понимаю почему не работает. Если когда-нибудь заработает - убрать следующую строчку
	gtk_container_set_border_width(dialog, 6);
	gtk_box_set_spacing(content_area, 6);
	/* Псевдоним */
	GtkLabel *buddy_name = gtk_label_new(mb->alias);
	gtk_box_pack_start(content_area, buddy_name, FALSE, TRUE, 0);
	/* Телефон */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(content_area, hbox, FALSE, TRUE, 0);
	GtkComboBox *phone_combo_box = gtk_combo_box_new_text();
	gtk_combo_box_append_text(phone_combo_box, mb->phones[0]);
	gtk_combo_box_append_text(phone_combo_box, mb->phones[1]);
	gtk_combo_box_append_text(phone_combo_box, mb->phones[2]);
	gtk_combo_box_set_active(phone_combo_box, 0);
	gtk_box_pack_start(hbox, gtk_label_new(_("Phone:")), FALSE, TRUE, 0);
	gtk_box_pack_start(hbox, phone_combo_box, TRUE, TRUE, 0);
	GtkButton *edit_phones_button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
	gtk_box_pack_end(hbox, edit_phones_button, FALSE, TRUE, 0);
	/* Текст сообщения */
	GtkScrolledWindow *scrolled_wnd = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(scrolled_wnd, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	GtkTextView *message_text = gtk_text_view_new();
	gtk_container_add(scrolled_wnd, message_text);
	gtk_box_pack_start(content_area, scrolled_wnd, TRUE, TRUE, 0);
	gtk_text_view_set_wrap_mode(message_text, GTK_WRAP_WORD);
	/* Флажок транслитерации и счётчик символов */
	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_spacing(hbox, 6);
	gtk_button_box_set_layout(hbox, GTK_BUTTONBOX_EDGE);
	GtkCheckButton *translit = gtk_check_button_new_with_label(_("Translit"));
	gtk_container_add(hbox, translit);
	GtkLabel *char_counter = gtk_label_new("");
	gtk_container_add(hbox, char_counter);
	gtk_box_pack_end(content_area, hbox, FALSE, TRUE, 0);
	/* Сохраним адреса нужных объектов */
	sms_dialog_params *params = g_new0(sms_dialog_params, 1);
	params->buddy = buddy;
	params->mrim = mrim;
	params->mb = mb;
	params->message_text = message_text;
	params->translit = translit;
	params->char_counter = char_counter;
	params->phone = phone_combo_box;
	params->sms_text = NULL;
	/* Подключим обработчики сигналов */
	g_signal_connect(dialog, "destroy", sms_dialog_destroy, params);
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(message_text);
		g_signal_connect(buffer, "changed", update_sms_char_counter, params);
		update_sms_char_counter(buffer, params);
	}
	g_signal_connect(translit, "toggled", update_sms_char_counter, params);
	g_signal_connect(dialog, "response", sms_dialog_response, params);
	g_signal_connect(edit_phones_button, "clicked", sms_dialog_edit_phones, params);
	/* Пока выключим транслит */
	gtk_widget_set_sensitive(translit, FALSE);
	/* Отображаем диалог */
	gtk_widget_show_all(dialog);
	/* Делаем активным окном окно ввода сообщения */
	gtk_widget_grab_focus(message_text);
}

// edit phones
void blist_edit_phones(PurpleBuddy *buddy, PurpleRequestFields *fields)
{
	// TODO PQ
	g_return_if_fail(buddy);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb);
	PurpleAccount *account = purple_buddy_get_account(buddy);
	PurpleConnection *gc = purple_account_get_connection(account);
	mrim_data *mrim = purple_connection_get_protocol_data(gc);

	PurpleRequestFieldGroup *group = fields->groups->data;
	purple_debug_info("mrim", "[%s] %s\n", __func__, group->title);
	gchar **phones = g_new0(gchar *, 4);
	phones[0] = g_strdup(purple_request_fields_get_string(fields, "phone1"));
	phones[1] = g_strdup(purple_request_fields_get_string(fields, "phone2"));
	phones[2] = g_strdup(purple_request_fields_get_string(fields, "phone3"));
	phones[3] = NULL;
	purple_debug_info("mrim", "[%s] %s %s %s\n", __func__, phones[0],phones[1],phones[2]);

	int i = 0;;
	while (phones[i])
	{
		FREE(mb->phones[i])
		mb->phones[i] = clear_phone(phones[i]);
		i++;
	}
	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = MODIFY_BUDDY;
	mpq->seq = mrim->seq;
	mpq->modify_buddy.buddy = buddy;
	mpq->modify_buddy.mb = mb;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
}
static void blist_edit_phones_menu_item(PurpleBlistNode *node, gpointer userdata)
{
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	mrim_data *mrim = userdata;
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(mrim != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	if (mb->phones == NULL)
		mb->phones = g_new0(char *, 4);

	// REQUEST
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(mb->addr);
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_string_new("phone1",_("_Main number"), mb->phones[0], FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("phone2",_("S_econd number"), mb->phones[1], FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("phone3",_("_Third number"), mb->phones[2], FALSE);
	purple_request_field_group_add_field(group, field);


	purple_request_fields(mrim->gc, _("Phone numbers editor"), _("Phone numbers editor"), _("Specify numbers as shown: +71234567890"),  fields,
			_("_Ok"), G_CALLBACK(blist_edit_phones),
			_("_Cancel"), NULL,
			mrim->account, buddy->name, NULL, buddy);
}

// VISIBLE / INVISIBLE
static void  blist_edit_invisible(PurpleBlistNode *node, gpointer userdata)
{
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	mrim_data *mrim = userdata;
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(mrim != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	mb->flags ^= CONTACT_FLAG_INVISIBLE;
	//mb->flags |= CONTACT_FLAG_SHADOW;

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = MODIFY_BUDDY;
	mpq->seq = mrim->seq;
	mpq->modify_buddy.mb = mb;
	mpq->modify_buddy.buddy = buddy;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
}
static void  blist_edit_visible(PurpleBlistNode *node, gpointer userdata)
{
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	mrim_data *mrim = userdata;
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(mrim != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	mb->flags ^= CONTACT_FLAG_VISIBLE;
	//mb->flags |= CONTACT_FLAG_SHADOW;

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = MODIFY_BUDDY;
	mpq->seq = mrim->seq;
	mpq->modify_buddy.mb = mb;
	mpq->modify_buddy.buddy = buddy;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
}

static void mrim_url_menu_action(PurpleBlistNode *node, gpointer userdata)
{
	// TODO code duplication
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	g_return_if_fail(buddy != NULL);
	mrim_data *mrim = purple_buddy_get_protocol_data(buddy);
	mrim_buddy *mb = buddy->proto_data;
	mrim_open_myworld_url(buddy->name, links[GPOINTER_TO_UINT(userdata)]);
}

// authorize
static void blist_authorize_menu_item(PurpleBlistNode *node, gpointer userdata)
{
/*	mrim_data *mrim = userdata;
	purple_request_input(mrim->gc,  "Requesting authorization", "Enter message for request authorization", NULL, "Please, authorize me", TRUE, FALSE,  NULL,
			"Request authorization", G_CALLBACK(_mrim_request_authorization_cb),
			"Cancel", NULL,
			md->account, name,  NULL, _mrim_auth_params_new(md, name) );
*/
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	g_return_if_fail(buddy != NULL);

	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	mrim_data *mrim = (mrim_data *) userdata;
	g_return_if_fail(mrim != NULL);

	purple_debug_info("mrim", "[%s] (%s) Asking authorization of <%s>\n", __func__, mrim->username, mb->addr);
	// Auth request.
	send_package_authorize(mrim, (gchar *)(mb->addr), (gchar *)(mrim->username));
}

static GList *mrim_user_actions(PurpleBlistNode *node)
{
	purple_debug_info("mrim", "[%s]\n", __func__);
	
	if (! PURPLE_BLIST_NODE_IS_BUDDY(node))
		return NULL;

	GList *list = NULL;
	list = g_list_append(list, NULL); // Separation line.

	PurpleMenuAction *action;
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	PurpleAccount *account = purple_buddy_get_account(buddy);
	PurpleConnection *gc = account->gc;
	mrim_data *mrim = gc->proto_data;
	mrim_buddy *mb = buddy->proto_data;

	if (mb)
	{
		if (! mb->authorized)
		{
			action = purple_menu_action_new(_("(Re-)request authorization"), PURPLE_CALLBACK(blist_authorize_menu_item), mrim, NULL);
			list = g_list_append(list, action);
		}

		if (mb->phones && mb->phones[0])
		{
			GHashTable *ht = purple_core_get_ui_info();
			gchar *name = g_hash_table_lookup(ht, "name");
			purple_debug_info("mrim", "[%s] UI is <%s>\n", __func__, name);
			if (name && g_strcmp0("Pidgin",name)==0) {
				action = purple_menu_action_new(_("Send an SMS... (GTK)"), PURPLE_CALLBACK(blist_sms_menu_item_gtk), mrim, NULL);
			//else
				//action = purple_menu_action_new(_("Send an SMS..."), PURPLE_CALLBACK(blist_sms_menu_item), mrim, NULL);
				list = g_list_append(list, action);
			}
			action = purple_menu_action_new(_("Send an SMS..."), PURPLE_CALLBACK(blist_sms_menu_item), mrim, NULL);
			list = g_list_append(list, action);
		}
		action = purple_menu_action_new(_("Edit phone numbers..."), PURPLE_CALLBACK(blist_edit_phones_menu_item), mrim, NULL);
		list = g_list_append(list, action);
		
		
		GList *private_list = NULL;
		action = purple_menu_action_new( (mb->flags & CONTACT_FLAG_INVISIBLE)? _("Remove from 'Invisible to' list"):_("Add to 'Invisible to' list"),
					 PURPLE_CALLBACK(blist_edit_invisible), mrim, NULL);
		private_list = g_list_append(private_list, action);

		action = purple_menu_action_new((mb->flags & CONTACT_FLAG_VISIBLE)?_("Remove from 'Visible to' list"):_("Add to 'Visible to' list"),
					PURPLE_CALLBACK(blist_edit_visible), mrim, NULL);
		private_list = g_list_append(private_list, action);
		
		action = purple_menu_action_new(_("Visibility settings"), NULL, mrim, private_list);
		list = g_list_append(list, action);

		if (mb->type == BUDDY)
		{
			// DO NOT REMOVE THIS CODE. WE *MUST* PROVIDE THIS LINKS DUE TO LICENSE AGREEMENT
			GList *myworld_list = NULL;
			action = purple_menu_action_new(_("[web] MY_WORLD"), PURPLE_CALLBACK(mrim_url_menu_action), GINT_TO_POINTER(MY_WORLD), NULL);
			myworld_list = g_list_append(myworld_list, action);
			action = purple_menu_action_new(_("[web] MY_PHOTO"), PURPLE_CALLBACK(mrim_url_menu_action), GINT_TO_POINTER(MY_PHOTO), NULL);
			myworld_list = g_list_append(myworld_list, action);
			action = purple_menu_action_new(_("[web] MY_VIDEO"), PURPLE_CALLBACK(mrim_url_menu_action), GINT_TO_POINTER(MY_VIDEO), NULL);
			myworld_list = g_list_append(myworld_list, action);
			action = purple_menu_action_new(_("[web] MY_BLOG"), PURPLE_CALLBACK(mrim_url_menu_action), GINT_TO_POINTER(MY_BLOG), NULL);
			myworld_list = g_list_append(myworld_list, action);
			action = purple_menu_action_new(_("My world"), NULL, mrim, myworld_list);
			list = g_list_append(list, action);
			// END MUST_HAVE CODE
		}
	}
	else
		; // mb should be created in add_buddy also.

	return list;
}

/******************************************
 *           MAIL
 ******************************************/
void notify_emails(void *gc, gchar* webkey, guint32 count)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	if (!purple_account_get_check_mail( ((PurpleConnection *)gc)->account ))
		return;
		
	mrim_data *mrim = ((PurpleConnection *)gc)->proto_data;
	gchar *url = NULL;
	if (webkey)
		url =  g_strdup_printf("http://win.mail.ru/cgi-bin/auth?Login=%s&agent=%s", mrim->username ,webkey);
	else
		url = g_strdup("mail.ru");
	
	char *mas[count], *mas_tos[count], *mas_urls[count];
	for (guint32 i=0; i<count; i++)
	{
		mas[i]=NULL;
		mas_tos[i] = mrim->username;
		mas_urls[i] = url;
	}
		/** Displays a notification for multiple emails to the user. **/
	purple_notify_emails(gc, count, FALSE, (const char **)mas, (const char **)mas, (const char **)mas_tos, (const char **)mas_urls, NULL, NULL);
}

/******************************************
 *           СТАТУСЫ
 ******************************************/
//Return list of supported statuses. (see status.h)
GList* mrim_status_types( PurpleAccount* account )
{
	purple_debug_info("mrim","[%s]\n",__func__);
	GList *status_list = NULL;
	PurpleStatusType *type;
	unsigned int i;
	for (i = 0; i < MRIM_PURPLE_STATUS_COUNT; i++) {
		type = purple_status_type_new_with_attrs(mrim_purple_statuses[i].primative, mrim_purple_statuses[i].id, _(mrim_purple_statuses[i].title), TRUE, mrim_purple_statuses[i].user_settable, FALSE, "message", _("Message"), purple_value_new(PURPLE_TYPE_STRING), NULL);
		status_list = g_list_append(status_list, type);
	}
	type = purple_status_type_new_with_attrs(PURPLE_STATUS_MOOD, "mood", NULL, FALSE, TRUE, FALSE, PURPLE_MOOD_NAME, _("Mood Name"), purple_value_new( PURPLE_TYPE_STRING ), PURPLE_MOOD_COMMENT, _("Mood Comment"), purple_value_new(PURPLE_TYPE_STRING), NULL);
	status_list = g_list_append(status_list, type);
	//type = purple_status_type_new_full(PURPLE_STATUS_MOBILE, MRIM_STATUS_ID_MOBILE, NULL, FALSE, FALSE, TRUE);
	//status_list = g_list_append(status_list, type);
	
	return status_list;
}

void free_mrim_status(mrim_status *status) {
	if (status) {
		FREE(status->uri)
		FREE(status->title)
		FREE(status->desc)
		FREE(status->display_string)
		//FREE(status->mood) /* TODO Memory leak */
	}
}

void make_mrim_status(mrim_status *s, guint32 status, gchar *uri, gchar *title, gchar *desc) {
	free_mrim_status(s);
	s->uri = uri;
	s->title = title;
	s->desc = desc;
	s->mood = NULL;
	unsigned int status_index = -1;
	if (uri) {
		unsigned int i;
		for (i = 0; i < MRIM_PURPLE_STATUS_COUNT; i++) {
			if (mrim_purple_statuses[i].uri) {
				if (strcmp(mrim_purple_statuses[i].uri, uri) == 0) {
					status_index = i;
					break;
				}
			}
		}
	}
	if (status_index == -1) {
		unsigned int i;
		for (i = 0; i < MRIM_PURPLE_STATUS_COUNT; i++) {
			if (mrim_purple_statuses[i].code !=  STATUS_USER_DEFINED) {
				if ((mrim_purple_statuses[i].code == status) || (mrim_purple_statuses[i].code & status)) {
					status_index = i;
					break;	
				}
			}
		}
		if (status_index == -1) {
			status_index = 1;
			if (uri) {
				for (i = 0; i < MRIM_PURPLE_MOOD_COUNT; i++) {
					if (strcmp(uri, mrim_purple_moods[i].uri) == 0) {
						s->mood = g_strdup(mrim_purple_moods[i].mood);
						break;
					}
				}
				if (!s->mood) {
					s->mood = g_strdup(s->uri);
				}
			}
		}
	}
	s->purple_status = mrim_purple_statuses[status_index].id;
	if (title && desc) {
		s->display_string = g_strdup_printf("%s - %s", title, desc);
	} else if (title) {
		s->display_string = g_strdup(title);
	} else if (desc) {
		s->display_string = g_strdup_printf("%s - %s", _(mrim_purple_statuses[status_index].title), desc);
	} else {
		s->display_string = g_strdup(_(mrim_purple_statuses[status_index].title));
	}
}

void make_mrim_status_from_purple(mrim_status *s, PurpleStatus *status) {
	const char *id = purple_status_type_get_id(purple_status_get_type(status));
	unsigned int status_index = -1, i;
	if (id) {
		for (i = 0; i < MRIM_PURPLE_STATUS_COUNT; i++) {
			if (mrim_purple_statuses[i].id) {
				if (strcmp(mrim_purple_statuses[i].id, id) == 0) {
					status_index = i;
					break;
				}
			}
		}
	}
	if (status_index == -1) {
		status_index = 1;
	}
	s->mood = g_strdup(purple_status_get_attr_string(status, PURPLE_MOOD_NAME));
	s->purple_status = g_strdup(mrim_purple_statuses[status_index].id);
	if (s->mood) {
		s->title = g_strdup(purple_status_get_attr_string(status, PURPLE_MOOD_COMMENT));
		s->code = STATUS_USER_DEFINED;
		s->uri = NULL;
		for (i = 0; i < MRIM_PURPLE_MOOD_COUNT; i++) {
			if (strcmp(s->mood, mrim_purple_moods[i].mood) == 0) {
				s->uri = g_strdup(mrim_purple_moods[i].uri);
				if (!s->title) {
					s->title = g_strdup(_(mrim_purple_moods[i].title));
				}
				break;
			}
		}
		if (!s->uri) {
			s->uri = g_strdup(s->mood);
			if (!s->title) {
				s->title = g_strdup(_(mrim_purple_statuses[status_index].title));
			}
		}
	} else {
		s->code = mrim_purple_statuses[status_index].code;
		s->uri = g_strdup(mrim_purple_statuses[status_index].uri);
		s->title = g_strdup(_(mrim_purple_statuses[status_index].title));
	}
	s->desc = purple_markup_strip_html(purple_status_get_attr_string(status, "message"));
}

static void mrim_set_status(PurpleAccount *acct, PurpleStatus *status)
{
	g_return_if_fail(status != NULL);
	g_return_if_fail(purple_account_is_connected(acct));

	PurpleConnection *gc = purple_account_get_connection(acct);
	mrim_data *mrim = gc->proto_data;

	gchar *prpl_status_id = purple_status_get_id(status);
	purple_debug_info("mrim", "[%s] setting %s's status to <%s, id %s>\n", __func__, acct->username, purple_status_get_name(status), prpl_status_id);

	make_mrim_status_from_purple(&mrim->status, status);
	package *pack = new_package(mrim->seq, MRIM_CS_CHANGE_STATUS);
	add_ul(mrim->status.code, pack);
	add_LPS(mrim->status.uri, pack);	// spec
	add_LPS(mrim->status.title, pack); // status title
	add_LPS(mrim->status.desc, pack); // desc or NULL
	add_ul(COM_SUPPORT, pack);
	send_package(pack, mrim);
}

char *mrim_status_text(PurpleBuddy *buddy) {
	mrim_buddy *mb = buddy->proto_data;
	if (mb) {
		return g_strdup(mb->status.display_string);
	}
	return NULL;
}

void set_user_status(mrim_data *mrim, gchar *email, guint32 status, gchar *uri, gchar *title, gchar *desc, gchar* user_agent)
{
	purple_debug_info("mrim", "[%s] %s changes status to 0x%x\n", __func__, email, status);
	g_return_if_fail(mrim != NULL);
	purple_debug_info("mrim", "[%s] %s user agent becomes %s\n", __func__, email, user_agent);

	PurpleBuddy *buddy = purple_find_buddy(mrim->account, email);
	g_return_if_fail(buddy != NULL);
	
	if (buddy && buddy->proto_data)
	{
		mrim_buddy *mb = buddy->proto_data;
		if (user_agent)
		{
			FREE(mb->user_agent);
			mb->user_agent = user_agent;
		} else
		{
			FREE(mb->user_agent);
			mb->user_agent = NULL;
		}
		
		make_mrim_status(&mb->status, status, uri, title, desc);
		
		purple_prpl_got_user_status(mrim->account, email, mb->status.purple_status, NULL);
		if (mb->status.mood) {
			purple_prpl_got_user_status(mrim->gc->account, mb->addr, "mood",
				PURPLE_MOOD_NAME, mb->status.mood,
				PURPLE_MOOD_COMMENT, mb->status.desc,
				NULL);
		} else {
			purple_prpl_got_user_status_deactive(mrim->gc->account, mb->addr, "mood");
		}
		
		if (!mb->authorized)
		{
			purple_prpl_got_user_status_deactive(mrim->gc->account, email, "mood");
			purple_prpl_got_user_status(mrim->account, email, "offline", NULL);
	        return;
		}
	} else {
		purple_prpl_got_user_status_deactive(mrim->gc->account, email, "mood");
		purple_prpl_got_user_status(mrim->account, email, "offline", NULL);
	}
}

void set_user_status_by_mb(mrim_data *mrim, mrim_buddy *mb)
{
	g_return_if_fail(mb);
	g_return_if_fail(mrim);
	PurpleAccount *account = mrim->account;
	if (mb->authorized) {
		purple_prpl_got_user_status(account, mb->addr, mb->status.purple_status, NULL);
		if (mb->status.mood) {
			purple_prpl_got_user_status(mrim->gc->account, mb->addr, "mood",
				PURPLE_MOOD_NAME, mb->status.mood,
				PURPLE_MOOD_COMMENT, mb->status.desc,
				NULL);
		} else {
			purple_prpl_got_user_status_deactive(mrim->gc->account, mb->addr, "mood");
		}
	} else {
		purple_prpl_got_user_status(account, mb->addr, "offline", NULL);
		purple_prpl_got_user_status_deactive(mrim->gc->account, mb->addr, "mood");
	}

	/* if (mb->phones && mb->phones[0])
		purple_prpl_got_user_status(account, mb->addr, MRIM_STATUS_ID_MOBILE, NULL);
	else
		purple_prpl_got_user_status_deactive(mrim->account, mb->addr, MRIM_STATUS_ID_MOBILE); */

	if (mb->flags & CONTACT_FLAG_PHONE)
		purple_prpl_got_user_status(account, mb->addr, "online", NULL);

}

/******************************************
 *              MICROBLOG
 ******************************************/
 
void set_buddy_microblog(mrim_data *mrim, gchar *email, gchar *microblog) {
	g_return_if_fail(mrim != NULL);
	PurpleBuddy *buddy = purple_find_buddy(mrim->account, email);
	g_return_if_fail(buddy != NULL);
	mrim_buddy *mb = buddy->proto_data;
	if (mb) {
		FREE(mb->microblog)
		mb->microblog = g_strdup(microblog);
	}
}

/******************************************
 *              LOGIN
 ******************************************/
static void mrim_prpl_login(PurpleAccount *account)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(account != NULL);

	PurpleConnection *gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);

	gc->flags |= PURPLE_CONNECTION_NO_FONTSIZE | PURPLE_CONNECTION_NO_URLDESC
			| PURPLE_CONNECTION_NO_IMAGES | PURPLE_CONNECTION_SUPPORT_MOODS
			| PURPLE_CONNECTION_SUPPORT_MOOD_MESSAGES;

	mrim_data *mrim = g_new0(mrim_data,1);
	mrim->gc = gc;
	mrim->fd = -1;
	mrim->account = account;
	mrim->username = g_strdup(purple_account_get_username(account));
	mrim->password = g_strdup(purple_account_get_password(account));
	mrim->mails = 0;
	mrim->web_key = NULL;
	mrim->error_count = 0;
	mrim->ProxyConnectHandle = NULL;
	make_mrim_status_from_purple(&mrim->status, purple_presence_get_active_status(account->presence));
	  
	mrim->server = g_strdup(purple_account_get_string(account, "balancer_host", MRIM_MAIL_RU));
	mrim->port = purple_account_get_int(account, "balancer_port", MRIM_MAIL_RU_PORT);
	mrim->user_agent = purple_account_get_bool(account, "use_custom_user_agent", FALSE) ?
		g_strdup(purple_account_get_string(account, "custom_user_agent", mrim_user_agent)) : g_strdup(mrim_user_agent); //TODO: Memory leak
	mrim->pq = g_hash_table_new_full(NULL,NULL, NULL, pq_free_element);
	mrim->mg = g_hash_table_new_full(NULL,NULL, NULL, mg_free_element);
	mrim->xfer_lists = NULL;
	gc->proto_data = mrim;

	char *endpoint = g_new0(gchar, strlen(mrim->server)+7);
	sprintf(endpoint, "%s:%i", mrim->server, mrim->port);
	purple_debug_info("mrim","[%s] EP=<%s>\n",__func__, endpoint);
	mrim->FetchUrlHandle = purple_util_fetch_url_request(endpoint, TRUE, NULL, FALSE, NULL, FALSE, mrim_balancer_cb, mrim);	// TODO mem leaks
	FREE(endpoint)
}  

void mrim_balancer_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	mrim_data* mrim = (mrim_data*)user_data;
	g_return_if_fail(mrim != NULL); // What if user disconnected?
	mrim->FetchUrlHandle = NULL;

	PurpleConnection *gc = mrim->gc;
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->proto_data != NULL);

    if ( len == 0 )
    {
		PurpleConnection* gc = purple_account_get_connection(mrim->account);
		purple_debug_error( "mrim", "[%s]: %s\n", __func__, error_message);
		purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,error_message);
        return;
    }
    purple_debug_info( "mrim", "[%s] Server -> %s\n", __func__, url_text);
    
    gchar** split = g_strsplit(url_text,":",2);// Split into two parts maximum.
	mrim->server = g_strdup(split[0]);
	mrim->port = atoi(g_strdup(split[1]));
	g_strfreev(split);
	
	//purple_debug_info( "mrim", " Server Parsed -> <%s> : <%i>\n",mrim->server, mrim->port);

	mrim->ProxyConnectHandle = purple_proxy_connect(mrim->gc,mrim->account, mrim->server, mrim->port, mrim_connect_cb, mrim->gc);
	if (! mrim->ProxyConnectHandle )
		purple_connection_error_reason (mrim->gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to create TCP-connection") );
}

void mrim_connect_cb(gpointer data, gint source, const gchar *error_message)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	PurpleConnection *gc = data;
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->proto_data!= NULL);

	mrim_data *mrim = gc->proto_data;
	if (mrim)
		mrim->ProxyConnectHandle = NULL;

	if ( (mrim == NULL) || (source < 0) )
	{
		gchar *tmp = g_strdup_printf(_("Unable to connect: %s"),	error_message);
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}
	mrim->fd = source;
	mrim->seq = 1;
	// We got working TCP-connection to mrim-server.
	// Send there MRIM_CS_HELLO
	purple_debug_info( "mrim", "Send MRIM_CS_HELLO\n");
	package *pack = new_package(mrim->seq, MRIM_CS_HELLO);
	if ( send_package(pack, mrim) )
	{
		purple_connection_update_progress(gc, _("Connecting"), 2/* current step */, 3/* steps total */);
		// Incoming traffic.
		gc->inpa = purple_input_add(mrim->fd, PURPLE_INPUT_READ, mrim_input_cb, gc);
	}
	else 
	{
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to write to socket."));
		purple_connection_set_state(gc, PURPLE_DISCONNECTED);
		return;
	}
}

/******************************************
 *              INPUT
 ******************************************/
void mrim_input_cb(gpointer data, gint source, PurpleInputCondition cond)
{	// TODO mem leaks
	// read_LPS
	// g_list_append
	// purple_buddy_new
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(source >= 0);
	PurpleConnection *gc = data;
	g_return_if_fail(gc != NULL);
	mrim_data *mrim = gc->proto_data;
	if (mrim == NULL)
	{
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_INVALID_SETTINGS, _("Disconnected by user action?"));
		purple_connection_set_state(gc, PURPLE_DISCONNECTED);
		//purple_account_disconnect(gc->account);
	}
	package *pack = read_package(mrim);
	if (pack == NULL)
	{
		int err;
		if (purple_input_get_error(mrim->fd, &err) != 0)
			purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Input Error"));

		mrim->error_count++; // TODO should we detect disconnects using fd?
		if (mrim->error_count > MRIM_MAX_ERROR_COUNT)
		{
			purple_debug_info("mrim", "Bad package\n");
			purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Bad Package"));
			//purple_connection_set_state(gc, PURPLE_DISCONNECTED);
			//purple_account_disconnect(gc->account);
		}
		return;
	}

	mrim_packet_header_t *header = pack->header;
	switch (header->msg)
	{
		case MRIM_CS_HELLO_ACK: { 
									gc->keepalive = (guint)read_UL(pack);
									mrim->kap_count = 0;
									purple_debug_info("mrim","KAP =<%u> \n",gc->keepalive);
									if (gc->keepalive > 0)
										{
											// LOGIN
											package *pack_ack = new_package(mrim->seq, MRIM_CS_LOGIN2);
											add_LPS(mrim->username, pack_ack);
											add_LPS(mrim->password, pack_ack);
											add_ul(mrim->status.code, pack_ack);
											add_LPS(mrim->status.uri, pack_ack);
											add_LPS(_(mrim->status.title), pack_ack);
											add_LPS(mrim->status.desc, pack_ack);
											add_ul(COM_SUPPORT /*FEATURES*/, pack_ack); // see mrim.h
											add_LPS(mrim->user_agent, pack_ack);
											add_LPS(_("ru"), pack_ack); // TODO: CS locale selection.
											add_LPS(DISPLAY_VERSION, pack_ack);

											send_package(pack_ack, mrim);
											// Keep Alive
											mrim->keep_alive_handle = purple_timeout_add_seconds(gc->keepalive,mrim_keep_alive,gc);
											if (!mrim->keep_alive_handle)
											{
												purple_debug_info("mrim", "Ping Eror\n");
												purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Ping Error") );
											}
										}
									break;
								}
		case MRIM_CS_LOGIN_ACK: {
									//purple_connection_update_progress(gc, "Connected", 3/* current step */, 3/* steps total */);
									purple_connection_set_state(gc, PURPLE_CONNECTED);
									purple_debug_info("mrim","LOGIN OK! \n");
									break;
								}
		case MRIM_CS_LOGIN_REJ: {
									purple_timeout_remove(mrim->keep_alive_handle);// No more KA sending.
									gc->wants_to_die = TRUE; // TODO
									mrim->keep_alive_handle = 0;
									purple_input_remove(gc->inpa); // No mo packets receiving.
									gc->inpa = 0;
									
									gchar *reason = read_LPS(pack);
									purple_debug_info("mrim","LOGIN REJ! <%s> \n",reason);
									gchar *tmp = g_strdup_printf(_("Disconnected. Reason: %s"),reason);
									purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, tmp);
									FREE(tmp)
									FREE(reason);
									break;
								}
		case MRIM_CS_MESSAGE_ACK:{
									purple_debug_info("mrim","MRIM_CS_MESSAGE_ACK!\n");
									mrim_read_im(mrim, pack);
									break;
								}
		case MRIM_CS_OFFLINE_MESSAGE_ACK:{// Сообщение было доставлено, пока пользователь не был в сети
									purple_debug_info("mrim","MRIM_CS_OFFLINE_MESSAGE_ACK!\n");
									guint32 first  = read_UL(pack);
									guint32 second = read_UL(pack);
									char *mes = read_LPS(pack);

									mrim_message_offline(gc, mes);

									// TODO Do we actually need it?
									// send MRIM_CS_OFFLINE_MESSAGE_ACK
									package *pack_ack = new_package(mrim->seq, MRIM_CS_DELETE_OFFLINE_MESSAGE);
									add_ul(first, pack_ack);
									add_ul(second, pack_ack);
									send_package(pack_ack, mrim);

									FREE(mes);
									break;
								}
		case MRIM_CS_USER_STATUS:{  // A buddy changed his status.
									gchar *uri=NULL, *title=NULL, *desc=NULL;
									guint32 status = read_UL(pack); //status
									if ( PROTO_MINOR(pack->header->proto) >= 0x14)
									{
										uri = read_LPS(pack);  // "status_22"
										title = read_LPS(pack); // "Работаю"
										desc = read_LPS(pack); // "Работаю не покладая рук"
									}
									gchar *user = read_LPS(pack); //user
									read_UL(pack);
									gchar *user_agent = read_LPS(pack);
									// Params:
									//  "client" - magent/jagent/??? -> mrim-get_ua_alias.
									//  "name" - sys-name.
									//  "title" - display-name.
									//  "version" - product internal numeration. Examples: "1.2", "1.3 pre".
									//  "build" - product internal numeration (may be positive number or time).
									//  "protocol" - MMP protocol number by format "<major>.<minor>".

									if (!user)
									{
										user = uri; // old proto
										uri = title = desc = NULL;
									}
									purple_debug_info("mrim","MRIM_CS_USER_STATUS! new_status<%i> user<%s> uri=<%s> title=<%s> desc=<%s> ua=<%s>\n", (int) status ,user, uri, title, desc, user_agent);
									set_user_status(mrim, user, status, uri, title, desc, user_agent);
									FREE(user);
									break;
								}
		case MRIM_CS_LOGOUT:	{
									purple_debug_info("mrim","MRIM_CS_LOGOUT! \n");
									guint32 reason = read_UL(pack);
									purple_timeout_remove(mrim->keep_alive_handle);// No more KA sending.
									mrim->keep_alive_handle = 0;// TODO logout
									purple_input_remove(gc->inpa);
									gc->inpa = 0;
									
									if (reason == LOGOUT_NO_RELOGIN_FLAG)
										purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NAME_IN_USE , _("Logged in from another location."));
									else 
										purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_OTHER_ERROR , _("Server broke connection."));
									//purple_connection_set_state(gc, PURPLE_DISCONNECTED);
									//purple_proxy_connect_cancel_with_handle(gc); // Disconnect from mrim.mail.ru
									//purple_input_remove(gc->inpa);
									break;
								}
		case MRIM_CS_CONNECTION_PARAMS:{ // Change connection properties (change KAP)
									gc->keepalive = read_UL(pack);
									purple_timeout_remove(gc->keepalive);
									mrim->keep_alive_handle = purple_timeout_add_seconds(gc->keepalive,mrim_keep_alive,gc);
									if (!mrim->keep_alive_handle)
									{
										purple_debug_info("mrim", "Ping Eror in MRIM_CS_CONNECTION_PARAMS\n");
										purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Ping Error in MRIM_CS_CONNECTION_PARAMS"));
									}
									purple_debug_info("mrim","MRIM_CS_CONNECTION_PARAMS!  KAP=<%i>\n",gc->keepalive);
									break;
								}
		case MRIM_CS_ADD_CONTACT_ACK:{// Acknowledge contact/group added.
									purple_debug_info("mrim","MRIM_CS_ADD_CONTACT_ACK!\n");
									mrim_add_contact_ack(mrim, pack);
									break;
								}
		case MRIM_CS_MODIFY_CONTACT_ACK:{
									purple_debug_info("mrim","MRIM_CS_MODIFY_CONTACT_ACK!\n");
									mrim_modify_contact_ack(mrim, pack);
									break;
								}
		case MRIM_CS_MESSAGE_STATUS:{ // Acknowledge message sent.
									purple_debug_info("mrim","MRIM_CS_MESSAGE_STATUS!  \n");
									mrim_message_status(pack);
									break;
								}
		case MRIM_CS_SMS_ACK:{
									purple_debug_info("mrim","MRIM_CS_SMS_ACK!\n");
									mrim_sms_ack(mrim, pack);
									break;
								}
		case MRIM_CS_AUTHORIZE_ACK:{
									purple_debug_info("mrim","MRIM_CS_AUTHORIZE_ACK!\n");
									gchar *from = read_LPS(pack);
									PurpleBuddy *buddy = purple_find_buddy(mrim->account, from);
									if (buddy && buddy->proto_data)
									{
										mrim_buddy *mb = buddy->proto_data;
										mb->authorized = TRUE;
									}
									break;
								}
		case MRIM_CS_CONTACT_LIST2:{  // Buddies list.
									switch (read_UL(pack))
									{
										case GET_CONTACTS_OK:
											mrim_cl_load(gc, mrim, pack);
											break;
										case GET_CONTACTS_ERROR:
											purple_debug_info("mrim","GET_CONTACTS_ERROR\n");
											break; // TODO disconnect?
										case GET_CONTACTS_INTERR:
											purple_debug_info("mrim","GET_CONTACTS_INTERR\n");
											break;// TODO disconnect?
										default:// TODO disconnect?
											break;
									}
									break;									
								}
		case MRIM_CS_USER_INFO:{ // Contact details.
									purple_debug_info("mrim","MRIM_CS_USER_INFO!\n");
									gchar *param=NULL, *value=NULL;
									while(TRUE)
									{
										param = read_LPS(pack);
										value = read_LPS(pack);
										if (! (param && value))
										{
											FREE(param)
											FREE(value)
											break;
										}

										if (strcmp(param, "MESSAGES.TOTAL") == 0 )
										{

										}
										else if (strcmp(param, "MESSAGES.UNREAD") == 0)
										{
											mrim->mails = atoi(value);
											mrim_pq *mpq = g_new0(mrim_pq, 1);
											mpq->type = NEW_EMAILS;
											mpq->seq = mrim->seq;
											mpq->new_emails.count = mrim->mails;
											g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);
										}
										else if (strcmp(param, "micblog.status.text")==0)
										{

										}
										else if (strcmp(param, "MRIM.NICKNAME")==0)
										{

										}
										// TODO
										// base64?? rb.target.cookie
										// lps timestamp
										// ul HAS_MYMAIL
										// lps(number) mrim.status.open_search
										// lps(ip:port) client.endpoint
										// lps connect.xml
										// lps(number) show_web_history_link
										// lps(number) friends_suggest

										FREE(param);
										FREE(value);
									}
									break;
								}
		case MRIM_CS_MAILBOX_STATUS:{ // New messages in mailbox count.
									mrim->mails = read_UL(pack);
									purple_debug_info("mrim","MRIM_CS_MAILBOX_STATUS! mails=<%u>\n", mrim->mails);
									mrim_pq *mpq = g_new0(mrim_pq, 1);
									mpq->type = NEW_EMAILS;
									mpq->seq = mrim->seq;
									mpq->new_emails.count = mrim->mails;
									g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);
									package *pack_ack = new_package(mpq->seq, MRIM_CS_GET_MPOP_SESSION);
									send_package(pack_ack, mrim);
									break;
								}
		case MRIM_CS_NEW_MAIL: {
									mrim->mails += read_UL(pack); //TODO += ??
									purple_debug_info("mrim","MRIM_CS_NEW_EMAIL! mails=<%u>\n", mrim->mails);
									mrim_pq *mpq = g_new0(mrim_pq, 1);
									mpq->type = NEW_EMAIL;
									mpq->seq = mrim->seq;
									mpq->new_email.from = read_LPS(pack);
									mpq->new_email.subject = read_LPS(pack);
									g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);
									package *pack_ack = new_package(mpq->seq, MRIM_CS_GET_MPOP_SESSION);
									send_package(pack_ack, mrim);
									break;
								}
		case MRIM_CS_MPOP_SESSION:{
									purple_debug_info("mrim","MRIM_CS_MPOP_SESSION\n"); 
									mrim_mpop_session(mrim, pack);
									break;
								}
		case MRIM_CS_FILE_TRANSFER:{
									purple_debug_info("mrim","MRIM_CS_FILE_TRANSFER\n");
#ifdef FT
									mrim_process_file_transfer(mrim, pack);
#else
									gchar *from = read_LPS(pack);

									guint32 session_id = read_UL(pack);
									//guint32 files_size = read_UL(pack);
									//read_UL(pack); // skip habracadabra bullshit XDD
									//gchar *files_info = read_LPS(pack);
									//gchar *more_file_info = read_UTF16LE(pack); // TODO FIX
									//gchar *ips = read_LPS(pack);

									package *pack_ack = new_package(mrim->seq, MRIM_CS_FILE_TRANSFER_ACK);
									add_ul(FILE_TRANSFER_STATUS_INCOMPATIBLE_VERS, pack_ack);
									add_LPS(from,pack_ack);
									add_ul(session_id,pack_ack);
									add_LPS(NULL, pack_ack);
									send_package(pack_ack,mrim);
#endif
									break;
									}
		case MRIM_CS_FILE_TRANSFER_ACK:{
									purple_debug_info("mrim","MRIM_CS_FILE_TRANSFER_ACK\n");
#ifdef FT
									mrim_process_file_transfer_ack(mrim ,pack);
#endif
									break;
									}
		case MRIM_CS_PROXY:{
									purple_debug_info("mrim","MRIM_CS_PROXY\n");
									break;
									}
		case MRIM_CS_PROXY_ACK:{
									purple_debug_info("mrim","MRIM_CS_PROXY_ACK\n");
									break;
									}
		case MRIM_CS_ANKETA_INFO:{
									purple_debug_info("mrim", "MRIM_CS_ANKETA_INFO\n");
									mrim_anketa_info(mrim, pack);
									break;
									}
		case MRIM_MICROBLOG_RECORD:
			{
				purple_debug_info("mrim", "MRIM_MICROBLOG_RECORD\n");
				read_UL(pack); /* Ненужное поле */
				gchar *email = read_LPS(pack);
				read_UL(pack); read_UL(pack); read_UL(pack); /* 12 ненужных байт */
				gchar *microblog = read_LPS(pack);
				read_UL(pack); /* Ещё одно ненужное поле */
				set_buddy_microblog(mrim, email, microblog);
				g_free(email);
				g_free(microblog);
				break;
			}
		default :	{
						purple_debug_info("mrim","Not recognized pack received! Type=<%i> len=<%i>\n",(int) header->msg, (int)header->dlen);
						//mrim_packet_dump(pack);
						break;
					}
	}
	free_package(pack);
	mrim->error_count = 0;
}




gboolean mrim_keep_alive(gpointer data)
{
	g_return_val_if_fail(data != NULL, FALSE);
	PurpleConnection *gc = data;
	g_return_val_if_fail(gc->state != PURPLE_DISCONNECTED, FALSE);
	mrim_data *mrim = gc->proto_data;
	purple_debug_info("mrim", "sending keep alive <%u>\n", mrim->seq);

	package *pack = new_package(mrim->seq, MRIM_CS_PING);
	send_package(pack, mrim);

	// Looking for lost packages in PQ
	/*
	GList *list;
	GList *first;
	list = first = g_hash_table_get_values(mrim->pq);
	while (list)
	{
		mrim_pq *mpq = list->data;
		g_return_if_fail(mpq != NULL);
		switch (mpq->type)
		{
			case MESSAGE: // Usually happens when a contact occidentally goes offline.
			{
				if (mpq->kap_count +4 < mrim->kap_count) // 2 minutes to get delivered.
				{
					// TODO should turn PQ to message_id
					//resend message
					//mrim_send_im(gc, mpq->message.to, mpq->message.message, mpq->message.flags);
					//g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(mpq->seq)); // This task will be added to PQ again.
				}
				break;
			}
			case AVATAR: // To lower the channel load.
			{
				// TODO Do we need it actually?
				break;
			}
		}
		list = list->next;
	}
	g_list_free(first);
	*/
	return TRUE; // Further KAP sending.
}

static void mrim_prpl_close(PurpleConnection *gc)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(gc != NULL);
	if (gc->inpa)
	{
		purple_input_remove(gc->inpa); // No more packages receiving.
		gc->inpa = 0;
	}

	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim != NULL);

	if (mrim->FetchUrlHandle)
	{
		purple_util_fetch_url_cancel(mrim->FetchUrlHandle);
		mrim->FetchUrlHandle = NULL;
	}
	if (mrim->ProxyConnectHandle)
	{
		purple_proxy_connect_cancel(mrim->ProxyConnectHandle);
		mrim->ProxyConnectHandle = NULL;
	}

	if (mrim->keep_alive_handle)
	{
		purple_timeout_remove(mrim->keep_alive_handle);
		mrim->keep_alive_handle = 0;
	}
	if (mrim->fd >= 0)
		close(mrim->fd);
	mrim->fd = -1;

	FREE(mrim->server)
	FREE(mrim->inp_package)
	FREE(mrim->web_key)
	FREE(mrim->url)
	FREE(mrim->user_agent)

	g_hash_table_remove_all(mrim->mg);
	g_hash_table_remove_all(mrim->pq);
	FREE(mrim)
	purple_connection_set_protocol_data(gc, NULL);

	// TODO stop SMS & phone_edit & ... pq
	purple_prefs_disconnect_by_handle(gc);
    purple_connection_set_state(gc, PURPLE_DISCONNECTED);
//	purple_dnsquery_destroy
}


static const char *mrim_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
	return "mrim";
}

static gboolean mrim_can_receive_file(PurpleConnection *gc,const char *who) 
{
#ifdef FT
	return TRUE;
#else
	return FALSE;
#endif
}
PurpleMood *mrim_get_moods(PurpleAccount *account)
{
	return moods;
}
/* mrim support offline messages */
static gboolean mrim_offline_message(const PurpleBuddy *buddy) 
{
	return TRUE;
}
const char *mrim_list_emblem(PurpleBuddy *b)
{
	g_return_val_if_fail(b, NULL);

	mrim_buddy *mb = purple_buddy_get_protocol_data(b);
	if (mb)
		if (!mb->authorized)
			return "not-authorized";
	return NULL;
}
static void mrim_prpl_destroy(PurplePlugin *plugin) 
{
	// TODO Should we remove 'action'-s?
	g_free(mrim_user_agent);
	purple_debug_info("mrim", "shutting down\n");
}

/******************************************
 *              SMS COMMAND
 ******************************************/
static gboolean mrim_load_plugin(PurplePlugin *plugin)
{
	return TRUE;
}

static gboolean mrim_unload_plugin(PurplePlugin *plugin)
{
	return TRUE;
}

static PurplePluginProtocolInfo prpl_info =
{
  OPT_PROTO_MAIL_CHECK | OPT_PROTO_UNIQUE_CHATNAME |
  OPT_PROTO_CHAT_TOPIC,                /* options */
  NULL,                                /* user_splits */
  NULL,                                /* protocol_options */
  {   /* icon_spec, a PurpleBuddyIconSpec */
      "png,jpg,gif",                   /* format */
      0,                               /* min_width */
      0,                               /* min_height */
      128,                             /* max_width */
      128,                             /* max_height */
      10000,                           /* max_filesize */
      PURPLE_ICON_SCALE_DISPLAY,       /* scale_rules */
  },
  mrim_list_icon,                      /** list_icon **/
  mrim_list_emblem,                    /* list_emblem */
  mrim_status_text,			                       /* status_text */
  mrim_tooltip_text,                   /* tooltip_text */
  mrim_status_types,                   /* status_types */
  mrim_user_actions,	               /* blist_node_menu */
  mrim_chat_info,                  	   /* chat_info */
  mrim_chat_info_defaults,   		   /* chat_info_defaults */
  mrim_prpl_login,                     /** login */
  mrim_prpl_close,                     /** close */
  mrim_send_im,      	               /* send_im */
  NULL,			                       /* set_info */
  mrim_send_typing,               	   /* send_typing */
  mrim_get_info,                       /* get_info */
  mrim_set_status,	                   /* set_status */
  NULL,     		                   /* set_idle */
  NULL,                                /* change_passwd */
  mrim_add_buddy,	                   /* add_buddy */
  NULL,			                	   /* add_buddies */
  mrim_remove_buddy,                   /* remove_buddy */
  NULL,				            	   /* remove_buddies */
  NULL,             			       /* add_permit */
  NULL,               				   /* add_deny */
  NULL,               				   /* rem_permit */
  NULL,             			       /* rem_deny */
  NULL,           					   /* set_permit_deny */
  mrim_chat_join,          			   /* join_chat */
  NULL,               				   /* reject_chat */
  mrim_get_chat_name,          		   /* get_chat_name */
  mrim_chat_invite,    				   /* chat_invite */
  mrim_chat_leave,     				   /* chat_leave */
  mrim_chat_whisper,     			   /* chat_whisper */
  mrim_chat_send,               	   /* chat_send */
  NULL,			                       /* keepalive */
  NULL,                  			   /* register_user */
  NULL, 				               /* устарела - get_cb_info */
  NULL,                                /* устарела - get_cb_away */
  mrim_alias_buddy,        			   /* alias_buddy */
  mrim_move_buddy,			           /* group_buddy */
  mrim_rename_group,                   /* rename_group */
  free_buddy,  		                   /* buddy_free */
  NULL,            				       /* convo_closed */
  mrim_normalize,          			   /* normalize */
  NULL, /*mrim_set_buddy_icon,*/       /* set_buddy_icon */
  mrim_remove_group,               	   /* remove_group */
  NULL,                                /* get_cb_real_name */
  mirm_set_chat_topic,	               /* set_chat_topic */
  NULL,                                /* find_blist_chat */
  NULL,         					   /* roomlist_get_list */
  NULL,            					   /* roomlist_cancel */
  NULL,   							   /* roomlist_expand_category */
  mrim_can_receive_file,           	   /* can_receive_file */
#ifdef FT
  mrim_send_file,                      /* send_file */
  mrim_xfer_new,                       /* new_xfer */
#else
  NULL,         			           /* send_file */
  NULL,			                       /* new_xfer */
#endif
  mrim_offline_message,                /* offline_message */
  NULL,                                /* whiteboard_prpl_ops */
  NULL,                                /* send_raw */
  NULL,                                /* roomlist_room_serialize */
  NULL,                                /* unregister_user */
  mrim_send_attention,                 /* send_attention */
  NULL,                                /* get_attention_types */
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >=5
  sizeof(PurplePluginProtocolInfo),    /* struct_size */
#else
  (gpointer) sizeof(PurplePluginProtocolInfo)
#endif
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 5
  NULL,								   /* get_account_text_table */
#endif
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 6
  NULL,                                 /* initiate_media */
  NULL,                                 /* get_media_caps */
#endif
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 7
  mrim_get_moods,  						/* get_moods */
  NULL,  								/* set_public_alias */
  NULL									/* get_public_alias */
#endif
};

static PurplePluginInfo info =
{
  PURPLE_PLUGIN_MAGIC,                                     /* magic */
  PURPLE_MAJOR_VERSION,                                    /* major_version */
  PURPLE_MINOR_VERSION,                                    /* minor_version */
  PURPLE_PLUGIN_PROTOCOL,                                  /* type */
  NULL,                                                    /* ui_requirement */
  0,                                                       /* flags */
  NULL,                                                    /* dependencies */
  PURPLE_PRIORITY_DEFAULT,                                 /* priority */
  MRIM_PRPL_ID,                                            /* id */
  "Mail.Ru Agent",                                         /* name */
  DISPLAY_VERSION,                                         /* version */
  SUMMARY,   						                       /* summary */
  DESCRIPTION,                                             /* description */
  NULL,                                                    /* author */
  "open-club.ru",                                          /* homepage */
  mrim_load_plugin,                                        /* load */
  mrim_unload_plugin,                                                    /* unload */
  mrim_prpl_destroy,	                                   /* destroy */
  NULL,                                                    /* ui_info */
  &prpl_info,                                              /* extra_info */
  NULL,                                                    /* prefs_info - PurplePluginUiInfo*/
  mrim_prpl_actions,                                       /* actions */
  NULL,                                                    /* padding... */
  NULL,
  NULL,
  NULL,
};

static void mrim_prpl_init(PurplePlugin *plugin)
{
	purple_debug_info("mrim", "starting up\n");
	
	gchar *purple_version = purple_core_get_version();
	GHashTable *ht = purple_core_get_ui_info();
	gchar *ui_name = g_hash_table_lookup(ht, "name");
	gchar *ui_version = g_hash_table_lookup(ht, "version");
	mrim_user_agent = g_strdup_printf("client=\"mrim-prpl\" version=\"%s/%s\" ui=\"%s %s\"", purple_version, DISPLAY_VERSION, ui_name, ui_version);
	{
		unsigned int i;
		moods = g_new0(PurpleMood, MRIM_PURPLE_MOOD_COUNT + 1);
		for (i = 0; i < MRIM_PURPLE_MOOD_COUNT; i++) {
			moods[i].mood = mrim_purple_moods[i].mood;
			moods[i].description = _(mrim_purple_moods[i].title);
		}
	}
	
	PurpleAccountOption *option_server = purple_account_option_string_new(_("Server"),"balancer_host",MRIM_MAIL_RU);
	prpl_info.protocol_options = g_list_append(NULL, option_server);
	PurpleAccountOption *option_port = purple_account_option_int_new(_("Port"), "balancer_port", MRIM_MAIL_RU_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_port);
	PurpleAccountOption *option_avatar = purple_account_option_bool_new(_("Load userpics"), "fetch_avatar", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_avatar);
	PurpleAccountOption *option_use_custom_ua = purple_account_option_bool_new(_("Use custom user agent string"), "use_custom_user_agent", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_use_custom_ua);
	PurpleAccountOption *option_custom_ua = purple_account_option_string_new(_("Custom user agent"), "custom_user_agent", mrim_user_agent);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_custom_ua);
	
    _mrim_plugin = plugin;

    // i18n
    plugin->info->summary = _(SUMMARY);
    plugin->info->description = _(DESCRIPTION);
}

PURPLE_INIT_PLUGIN(mrim, mrim_prpl_init, info)


/******************************************
 *         libpurple new API
 ******************************************/
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION <= 5
void *purple_connection_get_protocol_data(const PurpleConnection *connection)
{
	g_return_val_if_fail(connection != NULL, NULL);
	return connection->proto_data;
}
void purple_connection_set_protocol_data(PurpleConnection *connection, void *proto_data)
{
	g_return_if_fail(connection != NULL);
	connection->proto_data = proto_data;
}

gpointer purple_buddy_get_protocol_data(const PurpleBuddy *buddy)
{
	g_return_val_if_fail(buddy != NULL, NULL);
	return buddy->proto_data;
}

void purple_buddy_set_protocol_data(PurpleBuddy *buddy, gpointer data)
{
	g_return_if_fail(buddy != NULL);
	buddy->proto_data = data;
}
#endif
