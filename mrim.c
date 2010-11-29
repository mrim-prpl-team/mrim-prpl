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

static PurpleConnection *get_mrim_gc(const char *username)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	PurpleAccount *acct = purple_accounts_find(username, MRIM_PRPL_ID);
	if (acct && purple_account_is_connected(acct))
		return acct->gc;
	else
		return NULL;
}

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


gboolean is_valid_email(gchar *email)
{
	#define DOMIANS_COUNT 6
	const gchar *mrim_domains[] = {"mail.ru", "list.ru", "inbox.ru", "bk.ru", "corp.mail.ru", "chat.agent"};
	gboolean valid = FALSE;
  	unsigned int i;

	purple_debug_info("mrim ","[%s] <%s> \n", __func__, email);
    if (! purple_email_is_valid(email))
        return FALSE;

    /********/
    //return TRUE; // TODO ??
    /********/
  	// проверить домены
    char **emailv = g_strsplit(email, "@", 2);
  	for (i=0; i < DOMIANS_COUNT; i++)
  		if (strcmp(emailv[1], mrim_domains[i]) == 0)
  				valid = TRUE;

  	g_strfreev(emailv);
	return valid;
}

gboolean is_valid_phone(gchar *phone)
{
	g_return_val_if_fail(phone, FALSE);
	purple_debug_info("mrim","[%s] <%s>\n",__func__, phone);
	int length = 0;
	while(*phone)
	{
		g_return_val_if_fail(*phone <= '9' && *phone >= '0' , FALSE); // TODO буквы НЕДОПУСТИМЫ
		phone++;
		length++;
	}
	g_return_val_if_fail(length == 11, FALSE);
	return TRUE;
}

gchar *clear_phone(gchar *original_phone)
{
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
	// оставляем только цифры
	int j=0;
	gchar *correct_phone = g_new0(gchar, 12);
	while(*phone && j<11)
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
	if (is_valid_phone(correct_phone))
		return correct_phone;

	g_free(correct_phone);
	return NULL;
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
	const char *body;
	PurpleAccount *acct;

	purple_debug_info("mrim", "Fetching %s's user info for %s\n", username, gc->account->username);

	if (gc->state != PURPLE_CONNECTED)
	{
		char *msg = g_strdup_printf("%s не в сети.", gc->account->username);
		purple_notify_error(gc, "UserInfo", "UserInfo не доступно", msg);
		g_free(msg);
	}
	else if (!is_valid_email((gchar*)username))
	{
		PurpleNotifyUserInfo *info = purple_notify_user_info_new();
		purple_notify_user_info_add_pair(info, "UserInfo для телефонов и чатов получить нельзя", "");
		purple_notify_userinfo(gc, username, info, NULL, NULL);
	}
	else
	{
	    gchar** split = g_strsplit(username,"@",2);// разделить на максимум две части
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

/******************************************
 *             PRPL ACTION
 ******************************************/
/* Action List протокола (Учётные записи -> mailru -> ТУТ ЭТОТ СПИСОК) */
static void mrim_account_action(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	purple_debug_info("mrim","[%s]\n",__func__);
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

	field = purple_request_field_string_new("text_box_nickname","Ник","",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_first_name","Имя","",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_surname","Фамилия","",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_choice_new("radio_button_gender", "Пол", 0);
	purple_request_field_choice_add(field, "Любой"); // TODO может быть purple_request_field_list* ?
	purple_request_field_choice_add(field, "Мужской");
	purple_request_field_choice_add(field, "Женский");
	purple_request_field_group_add_field(group, field);
	/* country */
	/* region */
	/* city */
	/* birthday */
	/* zodiak */
	field = purple_request_field_string_new("text_box_age_from","Возраст от","",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_age_to","Возраст до","",FALSE);
	purple_request_field_group_add_field(group, field);
	/* webkam */
	/* gotov poboltat' */
	field = purple_request_field_bool_new("check_box_online","Online",FALSE);
	purple_request_field_group_add_field(group, field);



	purple_request_fields(mrim->gc, "Поиск контактов", NULL, NULL,  fields,
			"_Искать!", G_CALLBACK(blist_search),
			"Я _передумал!", NULL,
			mrim->account, mrim->username, NULL, mrim->gc );

}

static void mrim_easy_action(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *)action->context;
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	purple_debug_info("mrim","[%s]\n",__func__);
	gchar *p=NULL;
    gchar** split = g_strsplit(mrim->username,"@",2);
	gchar *name = g_strdup(split[0]);
	gchar *domain =p= g_strdup(split[1]);
	if (domain)
	{
		while(*domain)
			domain++;
		while((*domain != '.')&&(domain > p))
			domain--; // TODO segfault
		*domain = 0;
	}
	domain = p;
	g_strfreev(split);
	gchar *url = g_strdup_printf(links[GPOINTER_TO_UINT(action->user_data)], domain, name);
	purple_debug_info("mrim","[%s] d<%s> n<%s>\n",__func__, domain, name);
	purple_notify_uri(_mrim_plugin, url);
}

static GList *mrim_prpl_actions(PurplePlugin *plugin, gpointer context)
{	// TODO
	purple_debug_info("mrim","[%s]\n",__func__);
	PurplePluginAction *action = NULL;
	GList *actions = NULL;

	action = purple_plugin_action_new("[web]Set User Info", mrim_account_action);
	action->user_data = GUINT_TO_POINTER(MY_PROFILE);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new("[web]Set User avatar", mrim_account_action);
	action->user_data = GUINT_TO_POINTER(MY_AVATAR);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new("Искать собеседника", mrim_search_action);
	action->user_data = NULL;
	actions = g_list_append(actions, action);

	/*
	action = purple_plugin_action_new("[web]MY_WORLD", mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_WORLD);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new("[web]MY_PHOTO", mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_PHOTO);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new("[web]MY_VIDEO", mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_VIDEO);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new("[web]MY_BLOG", mrim_easy_action);
	action->user_data = GUINT_TO_POINTER(MY_BLOG);
	actions = g_list_append(actions, action);*/
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

	gc = get_mrim_gc(buddy->account->username);
	if (gc) 
	{
		/* they're logged in */
		if (buddy->alias)
			purple_notify_user_info_add_pair(info, "Имя", buddy->alias);
		
		PurplePresence *presence = purple_buddy_get_presence(buddy);
		PurpleStatus *status = purple_presence_get_active_status(presence);
		char *msg = g_strdup(""); // X-стаус?
		purple_notify_user_info_add_pair(info, purple_status_get_name(status), msg);
		g_free(msg);
		
		if (full)
		{
			const char *user_info = purple_account_get_user_info(account);
			if (user_info)
				purple_notify_user_info_add_pair(info, "Информация", user_info);
		}

		mrim_buddy *mb = buddy->proto_data;
		if (mb)
			if (mb->phones && mb->phones[0])
				purple_notify_user_info_add_pair(info, "Телефоны", mrim_phones_to_string(mb->phones));
	} 
	else
		purple_notify_user_info_add_pair(info, "Информация", "Не в сети");
}




/******************************************
 *             USER ACTION
 ******************************************/
static void blist_search(PurpleConnection *gc, PurpleRequestFields *fields)
{
	g_return_if_fail(gc);
	mrim_data *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = SEARCH;
	mpq->seq = mrim->seq;
	// добавлям mpq в PQ, только если отсылаем пакет
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
	while (index-- && list) // TODO может index-- ?
		list = list->next;

	gchar *message = purple_request_fields_get_string(fields, "message_box");
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
	field = purple_request_field_choice_new("combobox", "Выберите номер телефона", 0);
	purple_request_field_choice_add(field, mb->phones[0]); // TODO может быть purple_request_field_list* ?
	purple_request_field_choice_add(field, mb->phones[1]);
	purple_request_field_choice_add(field, mb->phones[2]);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("message_box","Текст СМС-сообщения","",TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(mrim->gc, "Отправка СМС", NULL, "Текст сообщения не должен превышать \n135 символов в английской раскладке \nили 35 символов в русской.",  fields,
			"_Отправить", G_CALLBACK(blist_send_sms),
			"Я _передумал!", NULL,
			mrim->account, buddy->name, NULL, mrim->gc );
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
	purple_debug_info("mrim", "[%s] change phones\n",__func__);
}
static void  blist_edit_phones_menu_item(PurpleBlistNode *node, gpointer userdata)
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
	field = purple_request_field_string_new("phone1","Ос_новной телефон", mb->phones[0], FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("phone2","За_пасной телефон", mb->phones[1], FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("phone3","_Ещё один телефон", mb->phones[2], FALSE);
	purple_request_field_group_add_field(group, field);



	purple_request_fields(mrim->gc, "Редактор телефонов", "Редактор телефонов", "Телефоны указываются в виде +7123456789",  fields,
			"_Ок", G_CALLBACK(blist_edit_phones),
			"О_тмена", NULL,
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

	// TODO  придумать хак =)
	guint32 state = mb->flags & CONTACT_FLAG_INVISIBLE;
	if (state)
		mb->flags &= !CONTACT_FLAG_INVISIBLE;
	else
		mb->flags |= CONTACT_FLAG_INVISIBLE;

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

	// TODO  придумать хак =)
	guint32 state = mb->flags & CONTACT_FLAG_VISIBLE;
	if (state)
		mb->flags &= !CONTACT_FLAG_VISIBLE;
	else
		mb->flags |= CONTACT_FLAG_VISIBLE;

	//mb->flags |= CONTACT_FLAG_SHADOW;

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = MODIFY_BUDDY;
	mpq->seq = mrim->seq;
	mpq->modify_buddy.mb = mb;
	mpq->modify_buddy.buddy = buddy;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
}


// authorize
static void blist_authorize_menu_item(PurpleBlistNode *node, gpointer userdata)
{
	purple_debug_info("mrim","[%s]\n",__func__);
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

	purple_debug_info("mrim", "[%s] Я(%s) прошу авторизацию у <%s>\n", __func__, mrim->username, mb->addr);
	// запрос авторизации
	send_package_authorize(mrim, (gchar *)(mb->addr), (gchar *)(mrim->username));
}

static GList *mrim_user_actions(PurpleBlistNode *node)
{
	purple_debug_info("mrim", "[%s]\n", __func__);
	
	if (! PURPLE_BLIST_NODE_IS_BUDDY(node))
		return NULL;

	GList *list = NULL;
	list = g_list_append(list, NULL); // разделительная черта

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
			action = purple_menu_action_new("(Повторно) Запросить авторизацию", PURPLE_CALLBACK(blist_authorize_menu_item), mrim, NULL);
			list = g_list_append(list, action);
		}

		if (mb->phones && mb->phones[0])
		{
			action = purple_menu_action_new("Отправить СМС...", PURPLE_CALLBACK(blist_sms_menu_item), mrim, NULL);
			list = g_list_append(list, action);
		}
		action = purple_menu_action_new("Редактировать телефоны...", PURPLE_CALLBACK(blist_edit_phones_menu_item), mrim, NULL);
		list = g_list_append(list, action);
		
		
		GList *private_list = NULL;
		action = purple_menu_action_new( (mb->flags & CONTACT_FLAG_INVISIBLE)? "Убрать из списка невидящих":"Добавить в список невидящих",
					 PURPLE_CALLBACK(blist_edit_invisible), mrim, NULL);
		private_list = g_list_append(private_list, action);

		action = purple_menu_action_new((mb->flags & CONTACT_FLAG_VISIBLE)?"Убарть из списка видящих":"Добавить в список видящих",
					PURPLE_CALLBACK(blist_edit_visible), mrim, NULL);
		private_list = g_list_append(private_list, action);
		
		action = purple_menu_action_new("Настройки видимости", NULL, mrim, private_list);
		list = g_list_append(list, action);
	}
	else
		; // mb должен быть создан ещё в add_buddy

	return list;
}

/******************************************
 *           ПОЧТА
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
	for (int i=0; i<count; i++)
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
	GList*	statuslist	= NULL;
	PurpleStatusType* type = NULL;
	unsigned int i;

	for ( i = 0; i < STATUSES_COUNT ; i++ )
	{
		const struct status* status = &mrim_statuses[i];
		//type = purple_status_type_new_with_attrs( status->primative, status->id, _( status->name ), TRUE, TRUE, FALSE,"message", _( "Message" ), purple_value_new( PURPLE_TYPE_STRING ),	NULL );
		type = purple_status_type_new_with_attrs( status->primative, status->id, status->name , TRUE, status->user_settable, status->independent, "message", "Message", purple_value_new( PURPLE_TYPE_STRING ), NULL );
		statuslist = g_list_prepend( statuslist, type );
	}

	/* add Mood option */
	//type = purple_status_type_new_with_attrs(PURPLE_STATUS_MOOD, "mood", NULL, FALSE, TRUE, TRUE,	PURPLE_MOOD_NAME, _("Mood Name"), purple_value_new( PURPLE_TYPE_STRING ), NULL);
	//type = purple_status_type_new_with_attrs(PURPLE_STATUS_MOOD, "mood", NULL, FALSE, TRUE, TRUE, PURPLE_MOOD_NAME, "Mood Name", purple_value_new( PURPLE_TYPE_STRING ), NULL);
	//statuslist = g_list_prepend( statuslist, type );

	/* add sms */
	type = purple_status_type_new_full(PURPLE_STATUS_MOBILE, MRIM_STATUS_ID_MOBILE, NULL, FALSE, FALSE, TRUE);
	statuslist = g_list_prepend(statuslist, type);

	return g_list_reverse(statuslist);
}

guint32 purple_status_to_mrim_status(PurpleStatus *status)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_val_if_fail(status != NULL, 0);
	PurpleStatusPrimitive primitive = purple_status_type_get_primitive(purple_status_get_type(status));
	unsigned int i;
	for ( i = 0; i < STATUSES_COUNT ; i++ )
		if ( mrim_statuses[i].primative == primitive )				/* status found! */
			return mrim_statuses[i].mrim_status;

	return STATUS_UNDETERMINATED;
}


const char* mrim_status_to_prpl_status( guint32 status )
{
	purple_debug_info("mrim","[%s] 0x%X\n",__func__, status);
	unsigned int	i;

	// TODO X-Status
	for ( i = 0; i < STATUSES_COUNT ; i++ )
		if ( ( mrim_statuses[i].mrim_status == status ) ||  // полное совпадение статуса
			 ( mrim_statuses[i].mrim_status & status ))     // "частичное" совпадение.
			return mrim_statuses[i].id;
		
	return "";
}


static void mrim_set_status(PurpleAccount *acct, PurpleStatus *status)
{
	g_return_if_fail(status != NULL);
	g_return_if_fail(purple_account_is_connected(acct));

	// В пиджине изменили статус
	const char *msg = purple_status_get_attr_string(status, "message");
	purple_debug_info("mrim", "setting %s's status to <%s>: %s\n", acct->username, purple_status_get_name(status), msg);

	PurpleConnection *gc = purple_account_get_connection(acct);
	mrim_data *mrim = gc->proto_data;

	package *pack = new_package(mrim->seq, MRIM_CS_CHANGE_STATUS);
	add_ul(purple_status_to_mrim_status(status), pack);
	add_LPS("X-status", pack);	// TODO ADD X-STATUS SETUP
	send_package(pack, mrim);
}

void set_user_status(mrim_data *mrim, gchar *email, guint32 status)
{
	purple_debug_info("mrim", "[%s] %s change status to 0x%x\n", __func__, email, status);
	g_return_if_fail(mrim != NULL);

	PurpleBuddy *buddy = purple_find_buddy(mrim->account, email);
	if (buddy && buddy->proto_data)
	{
		mrim_buddy *mb = buddy->proto_data;
		if (!mb->authorized)
		{
	        purple_prpl_got_user_status(mrim->account, email, "offline", NULL);
	        return;
		}
	}
    
	purple_prpl_got_user_status(mrim->account, email, mrim_status_to_prpl_status(status), NULL);
}

void set_user_status_by_mb(mrim_data *mrim, mrim_buddy *mb)
{
	g_return_if_fail(mb);
	g_return_if_fail(mrim);
	PurpleAccount *account = mrim->account;
	if (mb->authorized)
		purple_prpl_got_user_status(account, mb->addr, mrim_status_to_prpl_status(mb->status), NULL);
	else
		purple_prpl_got_user_status(account, mb->addr, "offline", NULL);

	if (mb->phones && mb->phones[0])
		purple_prpl_got_user_status(account, mb->addr, MRIM_STATUS_ID_MOBILE, NULL);
	else
		purple_prpl_got_user_status_deactive(mrim->account, mb->addr, MRIM_STATUS_ID_MOBILE);
	if (mb->flags & CONTACT_FLAG_PHONE)
		purple_prpl_got_user_status(account, mb->addr, "online", NULL);

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

	  
	mrim->server = g_strdup(purple_account_get_string(account, "balancer_host", MRIM_MAIL_RU));
	mrim->port = purple_account_get_int(account, "balancer_port", MRIM_MAIL_RU_PORT);
	mrim->pq = g_hash_table_new_full(NULL,NULL, NULL, pq_free_element);
	mrim->mg = g_hash_table_new_full(NULL,NULL, NULL, mg_free_element);
	gc->proto_data = mrim;

	char *endpoint = g_new0(gchar, strlen(mrim->server)+7);
	sprintf(endpoint, "%s:%i", mrim->server, mrim->port);
	purple_debug_info("mrim","[%s] EP=<%s>\n",__func__, endpoint);
	mrim->FetchUrlHandle = purple_util_fetch_url_request(endpoint, TRUE, NULL, FALSE, NULL, FALSE, mrim_balancer_cb, mrim);	// TODO mem leaks
	FREE(endpoint)
}  

static void mrim_balancer_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	mrim_data* mrim = (mrim_data*)user_data;
	g_return_if_fail(mrim != NULL); // вдруг юзер уже сделал дисконнект?
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
    
    gchar** split = g_strsplit(url_text,":",2);// разделить на максимум две части
	mrim->server = g_strdup(split[0]);
	mrim->port = atoi(g_strdup(split[1]));
	g_strfreev(split);
	
	//purple_debug_info( "mrim", " Server Parsed -> <%s> : <%i>\n",mrim->server, mrim->port);

	mrim->ProxyConnectHandle = purple_proxy_connect(mrim->gc,mrim->account, mrim->server, mrim->port, mrim_connect_cb, mrim->gc);
	if (! mrim->ProxyConnectHandle )
		purple_connection_error_reason (mrim->gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,"Не могу создать TCP-соединение");
}

static void mrim_connect_cb(gpointer data, gint source, const gchar *error_message)
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
		gchar *tmp = g_strdup_printf("Unable to connect: %s",	error_message);
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);
		g_free(tmp);
		return;
	}
	mrim->fd = source;
	mrim->seq = 1;
	// У нас есть рабочее TCP-соединение к mrim серверу.
	// Отправим туда MRIM_CS_HELLO
	purple_debug_info( "mrim", "Send MRIM_CS_HELLO\n");
	package *pack = new_package(mrim->seq, MRIM_CS_HELLO);
	if ( send_package(pack, mrim) )
	{
		purple_connection_update_progress(gc, "Connecting", 2/* текущий шаг*/, 3/*всего шагов*/);
		// входящий траффик
		gc->inpa = purple_input_add(mrim->fd, PURPLE_INPUT_READ, mrim_input_cb, gc);
	}
	else 
	{
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Не могу записать в сокет");
		purple_connection_set_state(gc, PURPLE_DISCONNECTED);
		return;
	}
}

/******************************************
 *              INPUT
 ******************************************/
static void mrim_input_cb(gpointer data, gint source, PurpleInputCondition cond)
{	// TODO устранить утечки памяти
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
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_INVALID_SETTINGS, "Соединение было разорвано пользователем?");
		purple_connection_set_state(gc, PURPLE_DISCONNECTED);
		//purple_account_disconnect(gc->account);
	}
	package *pack = read_package(mrim);
	if (pack == NULL)
	{
		int err;
		if (purple_input_get_error(mrim->fd, &err) != 0)
			purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Input Error");

		mrim->error_count+=1; // TODO может по fd определять дисконнект?
		if (mrim->error_count > MRIM_MAX_ERROR_COUNT)
		{
			purple_debug_info("mrim", "Bad package\n");
			purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Bad Package");
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
											mrim->status = STATUS_ONLINE;
											package *pack_ack = new_package(mrim->seq, MRIM_CS_LOGIN2);
											add_LPS(mrim->username, pack_ack);
											add_LPS(mrim->password, pack_ack);
											add_ul(mrim->status, pack_ack);
											add_LPS( USER_AGENT, pack_ack);
											send_package(pack_ack, mrim);
											// Keep Alive
											mrim->keep_alive_handle = purple_timeout_add_seconds(gc->keepalive,mrim_keep_alive,gc);
											if (!mrim->keep_alive_handle)
											{
												purple_debug_info("mrim", "Ping Eror\n");
												purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Ping Error");
											}
										}
									break;
								}
		case MRIM_CS_LOGIN_ACK: {
									//purple_connection_update_progress(gc, "Connected", 3/* текущий шаг*/, 3/*всего шагов*/);
									purple_connection_set_state(gc, PURPLE_CONNECTED);
									purple_debug_info("mrim","LOGIN OK! \n");
									break;
								}
		case MRIM_CS_LOGIN_REJ: {
									purple_timeout_remove(mrim->keep_alive_handle);// Больше не посылаем KA
									gc->wants_to_die = TRUE; // TODO
									mrim->keep_alive_handle = 0;
									purple_input_remove(gc->inpa); // больше не принимаем пакеты
									gc->inpa = 0;
									
									gchar *reason = read_LPS(pack);
									purple_debug_info("mrim","LOGIN REJ! <%s> \n",reason);
									gchar *tmp = g_strdup_printf("Disconnected. Причина: %s",reason); 
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

									// send MRIM_CS_OFFLINE_MESSAGE_ACK
									package *pack_ack = new_package(mrim->seq, MRIM_CS_DELETE_OFFLINE_MESSAGE);
									add_ul(first, pack_ack);
									add_ul(second, pack_ack);
									send_package(pack_ack, mrim);

									FREE(mes);
									break;
								}
		case MRIM_CS_USER_STATUS:{  // смена статуса другого пользователя
									guint32 status = read_UL(pack); //status
									gchar *user = read_LPS(pack); //user
									purple_debug_info("mrim","MRIM_CS_USER_STATUS! new_status<%i> user<%s>\n", (int) status ,user);
									set_user_status(mrim, user, status);
									FREE(user);
									break;
								}
		case MRIM_CS_LOGOUT:	{
									purple_debug_info("mrim","MRIM_CS_LOGOUT! \n");
									guint32 reason = read_UL(pack);
									purple_timeout_remove(mrim->keep_alive_handle);// Больше не посылаем KA .тип gboolean.
									mrim->keep_alive_handle = 0;// TODO logout
									purple_input_remove(gc->inpa);
									gc->inpa = 0;
									
									if (reason == LOGOUT_NO_RELOGIN_FLAG)
										purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NAME_IN_USE , "Аккаунт используется на другом компьютере");
									else 
										purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_OTHER_ERROR , "Сервер разорвал соединение");
									//purple_connection_set_state(gc, PURPLE_DISCONNECTED);
									//purple_proxy_connect_cancel_with_handle(gc); // отключаемся от mrim.mail.ru
									//purple_input_remove(gc->inpa);
									break;
								}
		case MRIM_CS_CONNECTION_PARAMS:{ // изменение параметров соединения (изменение KAP)
									gc->keepalive = read_UL(pack);
									purple_timeout_remove(gc->keepalive);
									mrim->keep_alive_handle = purple_timeout_add_seconds(gc->keepalive,mrim_keep_alive,gc);
									if (!mrim->keep_alive_handle)
									{
										purple_debug_info("mrim", "Ping Eror in MRIM_CS_CONNECTION_PARAMS\n");
										purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Ping Error in MRIM_CS_CONNECTION_PARAMS");
									}
									purple_debug_info("mrim","MRIM_CS_CONNECTION_PARAMS!  KAP=<%i>\n",gc->keepalive);
									break;
								}
		case MRIM_CS_ADD_CONTACT_ACK:{// подтверждение добавления контакта/группы
									purple_debug_info("mrim","MRIM_CS_ADD_CONTACT_ACK!\n");
									mrim_add_contact_ack(mrim, pack);
									break;
								}
		case MRIM_CS_MODIFY_CONTACT_ACK:{
									purple_debug_info("mrim","MRIM_CS_MODIFY_CONTACT_ACK!\n");
									mrim_modify_contact_ack(mrim, pack);
									break;
								}
		case MRIM_CS_MESSAGE_STATUS:{ // подтверждение доставки сообщения
									purple_debug_info("mrim","MRIM_CS_MESSAGE_STATUS!  \n");
									mrim_message_status(mrim, pack);
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
		case MRIM_CS_CONTACT_LIST2:{  // Контакт-лист
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
		case MRIM_CS_USER_INFO:{ // информация о пользователе
									purple_debug_info("mrim","MRIM_CS_USER_INFO!\n");
									gchar *param, *value;
									do
									{
										param = read_LPS(pack);
										value = read_LPS(pack);
										if (! (param && value))
											break;

										if (strcmp(param, "MESSAGES.UNREAD") == 0)
										{
											mrim_pq *mpq = g_new0(mrim_pq, 1);
											mpq->type = NEW_EMAILS;
											mpq->seq = mrim->seq;
											mpq->new_emails.count = atoi(value);
											g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);
										}
										// TODO
										// MESSAGES.TOTAL
										// MRIM.NICKNAME
										// base64?? rb.target.cookie
										// lps timestamp
										// ul HAS_MYMAIL
										// lps(число) mrim.status.open_search
										// lps(ip:port) client.endpoint
										// lps connect.xml
										// lps(число) show_web_history_link
										// lps(число) friends_suggest

										FREE(param);
										FREE(value);
									}while (param && value);
									break;
								}
		case MRIM_CS_MAILBOX_STATUS:{ // Количество писем в почтовом ящике
									purple_debug_info("mrim","MRIM_CS_MAILBOX_STATUS! mails=<%u>\n", mrim->mails);
									mrim_pq *mpq = g_new0(mrim_pq, 1);
									mpq->type = NEW_EMAILS;
									mpq->seq = mrim->seq;
									mpq->new_emails.count = mrim->mails;
									g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);
									package *pack = new_package(mpq->seq, MRIM_CS_GET_MPOP_SESSION);
									send_package(pack, mrim);
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
									package *pack = new_package(mpq->seq, MRIM_CS_GET_MPOP_SESSION);
									send_package(pack, mrim);
									break;
								}
		case MRIM_CS_MPOP_SESSION:{
									purple_debug_info("mrim","MRIM_CS_MPOP_SESSION\n"); 
									mrim_mpop_session(mrim, pack);
									break;
								}
		case MRIM_CS_FILE_TRANSFER:{
									purple_debug_info("mrim","MRIM_CS_FILE_TRANSFER\n");
									// TODO File Transfer
									gchar *from = read_LPS(pack);

									guint32 file_id = read_UL(pack);
									
									package *pack_ack = new_package(mrim->seq, MRIM_CS_FILE_TRANSFER_ACK);
									add_ul(FILE_TRANSFER_STATUS_INCOMPATIBLE_VERS, pack_ack);
									add_LPS(from,pack_ack);
									add_ul(file_id,pack_ack);
									add_LPS(NULL, pack_ack);
									send_package(pack_ack,mrim);
									FREE(from)
									break;
									}
		case MRIM_CS_FILE_TRANSFER_ACK:{
									purple_debug_info("mrim","MRIM_CS_FILE_TRANSFER_ACK\n");
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
		default :	{
						purple_debug_info("mrim","Пришёл неизвестный пакет! Type=<%i> len=<%i>\n",(int) header->msg, (int)header->dlen);
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

	// Теперь просматриваем PQ на предмет "пропавших" пакетов
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
			case MESSAGE: // обычно случается, когда контакт внезапно уходит в оффлайн
			{
				if (mpq->kap_count +4 < mrim->kap_count) // 2 минуты на доставку
				{
					// TODO сначала надо PQ перевести на message_id
					//resend message
					//mrim_send_im(gc, mpq->message.to, mpq->message.message, mpq->message.flags);
					//g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(mpq->seq)); // В PQ будет заново добавлено это задание
				}
				break;
			}
			case AVATAR: // для разгрузки канала
			{
				// TODO а оно надо ?
				break;
			}
		}
		list = list->next;
	}
	g_list_free(first);
	*/
	return TRUE; // посылать KAP и дальше
}

static void mrim_prpl_close(PurpleConnection *gc)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(gc != NULL);
	if (gc->inpa)
	{
		purple_input_remove(gc->inpa); // больше не принимаем пакеты
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
/* mrim doesn't support file transfer...yet... */
static gboolean mrim_can_receive_file(PurpleConnection *gc,const char *who) 
{
	return FALSE; // TODO
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
	// TODO может надо action-ы удалять?
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
  OPT_PROTO_MAIL_CHECK,                /* options */
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
  NULL,			                       /* status_text */
  mrim_tooltip_text,                   /* tooltip_text */
  mrim_status_types,                   /* status_types */
  mrim_user_actions,	               /* blist_node_menu */
  NULL,                  			   /* chat_info */
  NULL,         					   /* chat_info_defaults */
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
  NULL,                  			   /* join_chat */
  NULL,               				   /* reject_chat */
  NULL,              				   /* get_chat_name */
  NULL,               				   /* chat_invite */
  NULL,                				   /* chat_leave */
  NULL,               				   /* chat_whisper */
  NULL,                  			   /* chat_send */
  NULL,			                       /* keepalive */
  NULL,                  			   /* register_user */
  NULL, 				               /* устарела - get_cb_info */
  NULL,                                /* устарела - get_cb_away */
  mrim_alias_buddy,        			   /* alias_buddy */
  mrim_move_buddy,			           /* group_buddy */
  mrim_rename_group,                   /* rename_group */
  free_buddy,  		                   /* buddy_free */
  NULL,            				       /* convo_closed */
  NULL,                  			   /* normalize */
  NULL, /*mrim_set_buddy_icon,*/       /* set_buddy_icon */
  mrim_remove_group,               	   /* remove_group */
  NULL,                                /* get_cb_real_name */
  NULL,					               /* set_chat_topic */
  NULL,                                /* find_blist_chat */
  NULL,         					   /* roomlist_get_list */
  NULL,            					   /* roomlist_cancel */
  NULL,   							   /* roomlist_expand_category */
  mrim_can_receive_file,           	   /* can_receive_file */
  NULL,                                /* send_file */
  NULL,                                /* new_xfer */
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
  NULL,  								/* get_moods */
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
  "Mail.Ru Agent protocol plugin",                         /* summary */
  "Mail.Ru Agent protocol plugin",                         /* description */
  NULL,                                                    /* author */
  "open-club.ru",                                          /* homepage */
  mrim_load_plugin,                                        /* load */
  NULL,                                                    /* unload */
  mrim_prpl_destroy,	                                   /* destroy */
  NULL,                                                    /* ui_info */
  &prpl_info,                                              /* extra_info */
  NULL,                                                    /* prefs_info */
  mrim_prpl_actions,                                       /* actions */
  NULL,                                                    /* padding... */
  NULL,
  NULL,
  NULL,
};

static void mrim_prpl_init(PurplePlugin *plugin)
{
	purple_debug_info("mrim", "starting up\n");
	PurpleAccountOption *option_server = purple_account_option_string_new("Server","balancer_host",MRIM_MAIL_RU);
	prpl_info.protocol_options = g_list_append(NULL, option_server);
	PurpleAccountOption *option_port = purple_account_option_int_new("Port", "balancer_port", MRIM_MAIL_RU_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_port);
	PurpleAccountOption *option_avatar = purple_account_option_bool_new("Скачивать аватарки", "fetch_avatar", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option_avatar);
    _mrim_plugin = plugin;
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
