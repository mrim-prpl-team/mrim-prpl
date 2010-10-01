/**   cl.c of mrim-prpl project.
* Contact List management routines.
* Committed by Reslayer@mail.ru aka Reslayer.
*/
#include "cl.h"
/******************************************
 *        Загрузка Контакт Листа
 ******************************************/
static void cl_skeep(gchar *mask, package *pack)
{
	while (*mask)
		switch (*mask++ != '\0')
		{
			case 's': read_rawLPS(pack); break;
			case 'u': read_UL(pack); break;
			case 'z': read_Z(pack); break;
		}
}

void mrim_cl_load(PurpleConnection *gc, mrim_data *mrim, package *pack)
{	
	PurpleAccount *account = purple_connection_get_account(gc);
	guint32 g_number = read_UL(pack);// количество групп
	gchar *g_mask = read_LPS(pack); // маска групп
	gchar *c_mask = read_LPS(pack); // маска контактов

	purple_debug_info("mrim", "Маска группы <%s>, Маска контактов <%s>\n", g_mask, c_mask);

	/* группа Phone */
	mg_add(0, "phone", MRIM_PHONE_GROUP_ID, mrim);

	/** читаем группы **/
	u_long i = 0;
	for (i=0; i < g_number ; i++)
	{
		guint32 flags = read_UL(pack);//  & 0x00FFFFFF;
		gchar *name = read_LPS(pack); // группа (UTF16)
		//if (!(flags & CONTACT_FLAG_REMOVED))
			mg_add(flags, name, i, mrim);
		if (flags & CONTACT_FLAG_REMOVED)
			purple_debug_info("mrim","[%s] <%s> has flag REMOVED", __func__, name);

		cl_skeep(g_mask + 2, pack);
	}

	/** читаем пользователей **/
	int num = MRIM_MAX_GROUPS;
	while ( TRUE ) 
	{
		if (pack->cur >= pack->buf + pack->len)
			break; // просто убирает лишний мусор из дебага
		mrim_buddy *mb = new_mrim_buddy(pack);
		if (mb == NULL)
			break;
		mb->id = num++;
		purple_debug_info("mrim", "КОНТАКТ: Группа <%i>  E-MAIL <%s> NICK <%s> id <%i> status <%i> flags <%X>\n", mb->group_id, mb->addr, mb->alias, mb->id, (int)mb->status, mb->flags );
		if (mb->flags & CONTACT_FLAG_REMOVED)
			purple_debug_info("mrim","[%s] <%s> has flag REMOVED\n", __func__, mb->addr);

		if (!(mb->flags & CONTACT_FLAG_REMOVED)
				|| (purple_account_get_bool(account, "show_removed", FALSE)))
		{
			PurpleGroup *group = get_mrim_group_by_id(mrim, mb->group_id);
			PurpleBuddy *buddy = NULL;
			if (group)
			{	/*************/
				/* ADD BUDDY */
				/*************/
				// 1) Переименовываем телефонные контакты
				if (strcmp(mb->addr, "phone") == 0) // TODO подумать
				{
					purple_debug_info("mrim","[%s] rename phone buddy to %s\n",__func__, mb->phones[0]);
					g_free(mb->addr);
					mb->addr = g_strdup(mb->phones[0]);
					mb->status = STATUS_ONLINE;
					mb->flags |= CONTACT_FLAG_PHONE;
				}
				// 2) Если такой контакт уже был - прикурчиваем к нему
				//    иначе добавляем нового
				PurpleBuddy *old_buddy = purple_find_buddy(account, mb->addr);
				if (old_buddy != NULL)
				{
					purple_debug_info("mrim","Buddy <%s> already exsist!\n", old_buddy->name);
					// TODO переместить в нужную группу
					buddy = old_buddy;
				}
				else
				{
					purple_debug_info("mrim","Такого контакта ещё не было!\n");
					buddy = purple_buddy_new(gc->account, mb->addr, mb->alias);
					purple_blist_add_buddy(buddy, NULL/*contact*/, group, NULL/*node*/);
				}

				purple_buddy_set_protocol_data(buddy, mb);
				mb->buddy = buddy;
				if (! (mb->phones))
					mb->phones = g_new0(char *, 4);

				// псевдоним
				purple_blist_alias_buddy(buddy, mb->alias);
				//статус
				set_user_status_by_mb(mrim, mb);
				//аватарки
				if (purple_account_get_bool(account, "fetch_avatar", FALSE))
					mrim_fetch_avatar(buddy);// TODO где скачивать аватарки? // TODO PQ
			}
		}
		cl_skeep(c_mask+7, pack);
	}

	/* удаляем всех устаревших пользоватей */
	GSList *buddies = purple_find_buddies(gc->account, NULL);
	GSList *first = buddies;
	while (buddies)
	{
		PurpleBuddy *buddy = (PurpleBuddy*) (buddies->data);
		if (! (buddy->proto_data))
		{
			purple_debug_info("mrim","[%s] удаляю <%s>\n", __func__, buddy->name);
			purple_blist_remove_buddy(buddy);
		}
		buddies = g_slist_next(buddies);
	}
	g_slist_free(first);

	purple_blist_show();
	purple_debug_info("mrim","[%s]: Contact list loaded!\n", __func__);
	FREE(g_mask);
	FREE(c_mask);
}

static mrim_buddy *new_mrim_buddy(package *pack)
{
	mrim_buddy *mb = g_new(mrim_buddy, 1);
	mb->flags = read_UL(pack); // флаг
	mb->flags &= !CONTACT_FLAG_REMOVED;
	int gr_id = mb->group_id = read_UL(pack); // ID группы
	if (gr_id > MRIM_MAX_GROUPS)
		mb->group_id = MRIM_DEFAULT_GROUP_ID;
	mb->addr = read_LPS(pack); // аддрес контакта (UTF16LE)
	mb->alias = read_LPS(pack); // ник (UTF16LE)
	mb->s_flags= read_UL(pack); // серверный флаг (не авторизован)
	mb->status = read_UL(pack); // статус

	gchar *phones = read_LPS(pack); // телефон
	mb->phones = g_new0(char *, 4);
	//parse phones
	if (phones)
	{
		gchar **phones_splited = g_strsplit(phones, ",", 3);
		int i = 0;
		while (phones_splited[i])
		{
			mb->phones[i] = g_strdup_printf("+%s",phones_splited[i]);
			i++;
		}
		g_strfreev(phones_splited);
	}

	mb->authorized = !(mb->s_flags & CONTACT_INTFLAG_NOT_AUTHORIZED);
	// Переименовываем телефонные контакты
	if ( mb && (mb->flags & CONTACT_FLAG_PHONE) )
	{
		purple_debug_info("mrim","[%s] rename phone buddy\n",__func__);
		mb->addr = g_strdup(mb->phones[0]);
		mb->authorized = TRUE;
		mb->status = STATUS_ONLINE;
	}

	if (! mb->authorized)
		mb->status = STATUS_OFFLINE;

	if (mb->addr == NULL)
		return NULL;
	else
		return mb;
}

/******************************************
 *               Группы
 ******************************************/
// Запрос создания группы на сервере.
static void mrim_add_group(mrim_data *mrim, char *name)
{
	purple_debug_info("mrim","[%s] group_name=<%s>\n",__func__, name);

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = ADD_GROUP;
	mpq->seq = mrim->seq;
	mpq->add_group.name = name;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mrim->seq), mpq);

	mrim_pkt_add_group(mrim, name, mpq->seq);
	// TODO надо ли самому создавать группу?? или libpurple сделает это самостоятельно?
}

void mrim_rename_group(PurpleConnection *gc, const char *old_name, PurpleGroup *group, GList *moved_buddies)
{
	purple_debug_info("mrim", "[%s] group %s renamed to %s\n", __func__, old_name, group->name);
	mrim_data *mrim = gc->proto_data;

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = RENAME_GROUP;
	mpq->seq = mrim->seq;
	mpq->rename_group.new_group = group;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq ), mpq);

	int g_count = g_hash_table_size (mrim->mg);
	int group_id = get_mrim_group_id_by_name(mrim, (gchar *)old_name);
	if (group_id == MRIM_NO_GROUP);
	{
		purple_notify_warning(_mrim_plugin, "Работа с контакт-листом завершилась ошибкой!", "Работа с контакт-листом завершилась ошибкой!", "Группа не найдена");
		return;
	}
	mrim_group *mg = g_hash_table_lookup(mrim->mg, GUINT_TO_POINTER(group_id));
	guint32 flags = CONTACT_FLAG_GROUP;
	if (mg)
	{
		flags = mg->flag;
		//меняем mrim->mg
		mg->name = group->name;
		mg->gr = group;
	}

	mrim_pkt_modify_group(mrim, group_id, group->name, flags);
	// TODO надо переносить юзеров?
}

void mrim_remove_group(PurpleConnection *gc, PurpleGroup *group) 
{
	purple_debug_info("mrim", "[%s] remove group %s\n",__func__, group->name);
	mrim_data *mrim = gc->proto_data;

	guint32 group_id = get_mrim_group_id_by_name(mrim, group->name);
	if (group_id == MRIM_NO_GROUP)
	{
		purple_debug_info("mrim", "[%s] group %s not found\n",__func__, group->name);
		return;
	}

	mrim_group *mg = g_hash_table_lookup(mrim->mg, GUINT_TO_POINTER(group_id));
	guint32 flags;
	if (mg)
		flags = mg->flag;
	else
		flags = CONTACT_FLAG_GROUP;

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = REMOVE_GROUP;
	mpq->seq = mrim->seq;
	mpq->remove_group.group_name = g_strdup(group->name);
	mpq->remove_group.group_id = group_id;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mrim->seq), mpq);

	mrim_pkt_modify_group(mrim, group_id, group->name, flags | CONTACT_FLAG_REMOVED);
}


// MG
static void mg_add(guint32 flags, gchar *name, guint id, mrim_data *mrim)
{
	mrim_group *mg = g_new0(mrim_group, 1);
	mg->flag = flags;
	mg->name = name;
	mg->id = id;
	PurpleGroup *gr = purple_find_group(mg->name);
	if (gr == NULL)
	{
		gr = purple_group_new(mg->name);
		purple_blist_add_group(gr, NULL);
	}
	mg->gr = gr;
	purple_debug_info("mrim", "[%s] Группа id=<%u> flag=<%x> <%s>\n", __func__, mg->id, mg->flag, mg->name);
	g_hash_table_insert(mrim->mg, GUINT_TO_POINTER(id), mg);
}
// поиск группы по её id
PurpleGroup *get_mrim_group_by_id(mrim_data *mrim, guint32 id)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	mrim_group *mg =  g_hash_table_lookup(mrim->mg, GUINT_TO_POINTER(id));
	g_return_val_if_fail(mg != NULL, NULL);
	if (mg->gr)
		purple_debug_info("mrim", "Found grp %s, ID <%u> \n", mg->gr->name, id);
	else
		purple_debug_info("mrim", "Not found group by ID <%u>\n", id);
	return mg->gr;
}
// поиск группы по её названию
guint32 get_mrim_group_id_by_name(mrim_data *mrim, char *name)
{
	purple_debug_info("mrim","[%s]\n",__func__);

	GList *g = g_list_first(g_hash_table_get_values(mrim->mg));
	mrim_group *mg = NULL;
	while (g)
	{
		mg = g->data;
		if ( g_strcmp0(mg->name, name) == 0 )
		{
			purple_debug_info("mrim", "Found grp %s, ID %u\n", mg->name, mg->id);
			g_list_free(g);
			return mg->id;
		};
		g = g_list_next(g);
	};
	g_list_free(g);
	purple_debug_info("mrim", "Not found grp by alias, returning NO_GROUP ID <%u>\n", MRIM_NO_GROUP);
	return MRIM_NO_GROUP;
}

/******************************************
 *             Контакты
 ******************************************/
void mrim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	/* Note that when purple load local cached buddy list into its blist
	 * it also calls this funtion, so we have to define
	 * gc->state=PURPLE_CONNECTED AFTER LOGIN */
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(group != NULL);
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->state == PURPLE_CONNECTED);
	purple_debug_info("mrim","[%s] Add buddy <%s> into <%s> GROUP\n",__func__, buddy->name, group->name);

	// 1) Переименовываем телефонные контакты
	gchar *phone = clear_phone(buddy->name);
	if (is_valid_phone(phone))
	{
		purple_debug_info("mrim","[%s] rename phone buddy\n",__func__);
		g_free(buddy->name);
		buddy->name = phone;
	}
	else
		FREE(phone)

	mrim_buddy *mb;
	mrim_data *mrim = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBuddy *old_buddy = purple_find_buddy(account, buddy->name);

	/* 2) если такой контакт уже был - обновим. не был - добавим  */
	if (old_buddy != NULL  && old_buddy != buddy)
	{
		purple_debug_info("mrim","Buddy <%s> already exsist!\n", old_buddy->name);
		//purple_buddy_destroy(buddy);
		purple_blist_remove_buddy(buddy);
		buddy = old_buddy;
		mb = (mrim_buddy*)(buddy->proto_data);

		if (mb)
		{
			purple_debug_info("mrim","[%s] mb exsist\n",__func__);
			mb->buddy = buddy;
			purple_blist_alias_buddy(buddy, mb->alias);
			set_user_status_by_mb(mrim, mb);
		}
	}
	else
	{
		purple_debug_info("mrim","Такого контакта ещё не было!\n");
		// PQ
		mrim_pq *mpq = g_new0(mrim_pq, 1);
		mpq->type = ADD_BUDDY;
		mpq->seq = mrim->seq;
		mpq->add_buddy.buddy = buddy;
		mpq->add_buddy.group = group;

		guint32 group_id = get_mrim_group_id_by_name(mrim, group->name);
		if (group_id > MRIM_MAX_GROUPS)
		{
			mpq->add_buddy.group_exsist = FALSE;
			purple_debug_info("mrim","[%s] group not found! create new\n",__func__);
			mrim_pkt_add_group(mrim, group->name, mpq->seq);
		}
		else
		{
			purple_debug_info("mrim","[%s] group was found. Add buddy <%s>\n",__func__, buddy->name);
			mpq->add_buddy.group_exsist = TRUE;
			mb = g_new0(mrim_buddy, 1);
			mb->phones = g_new0(gchar *, 4);
			purple_buddy_set_protocol_data(buddy, mb);

			purple_blist_add_buddy(buddy, NULL, group, NULL); // добавляем, только если группа существует
			clean_string(buddy->name);
			if (is_valid_email(buddy->name))
			{
				purple_debug_info("mrim","[%s] it is email\n",__func__);
				mpq->add_buddy.authorized = FALSE;
				mb->addr = g_strdup(buddy->name);
				mb->authorized = FALSE;
				mb->group_id = group_id;
				mb->flags = 0;

				gchar *text = "Здравствуйте. Пожалуйста, добавьте меня в список ваших контактов.";
				gchar *ctext = g_convert(text, -1, "CP1251" , "UTF8", NULL, NULL, NULL);
				gchar *who = (buddy->alias)?(buddy->alias):(buddy->name);
				gchar *cwho = g_convert(who, -1, "CP1251" , "UTF8", NULL, NULL, NULL);

				package *pack = new_package(mpq->seq, MRIM_CS_ADD_CONTACT);
				add_ul(0, pack); // просто добавляем
				add_ul(group_id, pack);
				add_LPS(buddy->name, pack);
				add_LPS(who, pack); // псевдоним(ник/алиас)
				add_ul(0, pack); // null lps - телефоны
				add_base64(pack, FALSE, "uss", 2, mrim->username, ctext); //TODO сообщение авторизации
				add_ul(0, pack);
				send_package(pack, mrim);
			}
			if (is_valid_phone(buddy->name))
			{
				purple_debug_info("mrim","[%s] it is phone\n",__func__);
				mpq->add_buddy.authorized = TRUE;
				mb->phones[0] = g_strdup(buddy->name);
				mb->flags = CONTACT_FLAG_PHONE;
				mb->authorized = TRUE;
				mb->group_id = MRIM_PHONE_GROUP_ID;
				mb->addr = g_strdup("phone");
				mb->status = STATUS_ONLINE;

				gchar *text = "Здравствуйте. Пожалуйста, добавьте меня в список ваших контактов.";
				gchar *ctext = g_convert(text, -1, "CP1251" , "UTF8", NULL, NULL, NULL);
				gchar *who = (buddy->alias)?(buddy->alias):(buddy->name);
				gchar *cwho = g_convert(who, -1, "CP1251" , "UTF8", NULL, NULL, NULL);

				package *pack = new_package(mpq->seq, MRIM_CS_ADD_CONTACT);
				add_ul(CONTACT_FLAG_PHONE, pack); // просто добавляем
				add_ul(MRIM_PHONE_GROUP_ID, pack);
				add_LPS(mb->addr, pack);
				add_LPS((buddy->alias)?(buddy->alias):NULL, pack); // псевдоним(ник/алиас)
				add_LPS(mrim_phones_to_string(mb->phones), pack);
				add_base64(pack, FALSE, "uss", 2, cwho, ctext); //TODO сообщение авторизации
				add_ul(0, pack);
				send_package(pack, mrim);
			}
		}
		g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);
	}
	if (purple_account_get_bool(account, "fetch_avatar", FALSE))
		mrim_fetch_avatar(buddy); // TODO PQ
	purple_blist_show();
}

void mrim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(group != NULL);
	g_return_if_fail(gc != NULL);
	
	purple_debug_info("mrim", "[%s]\n",__func__);
	mrim_data *mrim = gc->proto_data;
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL); // TODO вернуть контакт обратно в контакт лист??
	
	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = REMOVE_BUDDY;
	mpq->seq = mrim->seq;
	mpq->remove_buddy.buddy = buddy;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mb->flags |= CONTACT_FLAG_REMOVED;
	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
	purple_debug_info("mrim", "[%s]removing %s from %s's buddy list. id=<%u> group_id=<%u>\n",__func__,buddy->name, gc->account->username, mb->id, mb->group_id);
}


static void free_buddy_proto_data(PurpleBuddy *buddy)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(buddy->proto_data != NULL);

	return ;// TODO
	mrim_buddy *mb = (mrim_buddy *) (buddy->proto_data);
	if (mb->phones)
		g_strfreev(mb->phones);
	FREE(mb->addr)
	FREE(mb->alias)
}

void free_buddy(PurpleBuddy *buddy)
{
	purple_debug_info("mrim","[%s]\n",__func__);
	g_return_if_fail(buddy != NULL);
	//free_buddy_proto_data(buddy);
	//FREE(buddy)
}
/** save/store buddy's alias on server list/roster */
void mrim_alias_buddy(PurpleConnection *gc, const char *who, const char *alias)
{
	purple_debug_info("mrim", "[%s] buddy=<%s>  new_alias=<%s>\n", __func__, who, alias);
	mrim_data *mrim = gc->proto_data;
	PurpleBuddy *buddy = purple_find_buddy(gc->account, who);
	g_return_if_fail(buddy != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);
	mb->alias = (gchar *)alias; // TODO strdup ?

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = MODIFY_BUDDY;
	mpq->seq = mrim->seq;
	mpq->modify_buddy.mb = mb;
	mpq->modify_buddy.buddy = buddy;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
}
/** change a buddy's group on a server list/roster */
void mrim_move_buddy(PurpleConnection *gc, const char *who, const char *old_group, const char *new_group)
{
	purple_debug_info("mrim", "[%s] move buddy=<%s> to <%s> group\n", __func__, who, new_group);
	mrim_data *mrim = gc->proto_data;

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = MOVE_BUDDY;
	mpq->seq = mrim->seq;
	mpq->move_buddy.buddy_name = (gchar *) who;
	mpq->move_buddy.new_group = (gchar *) new_group;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	int group_id = get_mrim_group_id_by_name(mrim, (gchar *) new_group);
	if (group_id == MRIM_NO_GROUP)
	{
		// добавим группу
		purple_debug_info("mrim","[%s] group not found! create new\n",__func__);
		mrim_pkt_add_group(mrim, (gchar *) new_group, mpq->seq);
	}
	else
	{
		PurpleBuddy *buddy = purple_find_buddy(gc->account, who);
		g_return_if_fail(buddy != NULL);
		mrim_buddy *mb = buddy->proto_data;
		g_return_if_fail(mb != NULL);
		mb->group_id = group_id;
		mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
	}
}
/******************************************
 *               Аватарки
 ******************************************/
static void mrim_fetch_avatar(PurpleBuddy *buddy)
{
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(buddy->name != NULL);
	purple_debug_info("mrim","[%s] <%s>\n",__func__, buddy->name);
	if (! is_valid_email(buddy->name))
		return;
	if ((!buddy->icon) && buddy->name)
	{
		// TODO грузить аватарки только в случае их отсутствия
		gchar** split_1 = g_strsplit(buddy->name,"@",2);
		gchar* email_name=split_1[0];
		gchar* domain;
		gchar** split_2;
		if (split_1[1])
		{
			split_2 = g_strsplit(split_1[1],".ru\0",2);
			domain = split_2[0];
		}
		else
			return;

		purple_debug_info("mrim","[%s] <%s>  <%s>\n", __func__, email_name, domain);

		gchar* url=g_strconcat("http://obraz.foto.mail.ru/",domain,"/",email_name,"/_mrimavatar",NULL);
		//mrim->FetchUrlHandle =
		purple_util_fetch_url(url,TRUE,USER_AGENT,TRUE, mrim_avatar_cb,buddy);
		FREE(url);
		g_strfreev(split_1);
		g_strfreev(split_2);
	}
}

static void mrim_avatar_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
{
	if(user_data==NULL)
		return;

    PurpleBuddy* pb=(PurpleBuddy*)user_data;
    
    if(url_text==NULL || len==0)
    {
        purple_debug_error("mrim","mrim_avatar_cb: Wrong avatar for %s:%s\n",purple_buddy_get_name(pb),(error_message==NULL)? "(null)":error_message);
        return;
    }
   // mrim->FetchUrlHandle = NULL;
    purple_buddy_icons_set_for_user(purple_buddy_get_account(pb),purple_buddy_get_name(pb),g_memdup(url_text, len),len,NULL);
}

/******************************************
 *               Авторизация
 ******************************************/
void mrim_authorization_yes(void *va_data)
{
	auth_data *a_data = (auth_data *) va_data;
	mrim_data *mrim = a_data->mrim;
	purple_debug_info("mrim","[%s] from=<%s>\n", __func__, a_data->from);
	package *pack = new_package(a_data->seq, MRIM_CS_AUTHORIZE);
	add_LPS(a_data->from, pack);
	send_package(pack, mrim);

	PurpleBuddy *buddy = purple_find_buddy(mrim->account, a_data->from);
	if (buddy && buddy->proto_data)
	{
		mrim_buddy *mb = buddy->proto_data;
		if (! mb->authorized)
			send_package_authorize(mrim, a_data->from, mrim->username);
	}

	g_free(a_data->from);
	g_free(a_data);
}

void mrim_authorization_no(void *va_data)
{	// ничего не отправляем, просто освобождаем память
	purple_debug_info("mrim","[%s]\n", __func__);
	auth_data *a_data = (auth_data *) va_data;
	g_free(a_data->from);
	g_free(a_data);
}



/******************************************
 *               Очереди
 ******************************************/
void mrim_add_contact_ack(mrim_data *mrim ,package *pack)
{
	purple_debug_info("mrim","[%s] seq=<%u>\n",__func__, pack->header->seq);
	guint32 status = read_UL(pack);
	guint32 id =read_UL(pack);
	if (status != CONTACT_OPER_SUCCESS)
		print_cl_status(status);
	g_return_if_fail(status == CONTACT_OPER_SUCCESS);

	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
	if (mpq == NULL)
		purple_notify_warning(_mrim_plugin, "Работа с контакт-листом завершилась ошибкой!", "Работа с контакт-листом завершилась ошибкой!", "Такая операция не осуществлялась? (mpq == NUL)");
	g_return_if_fail(mpq);
	switch (mpq->type)
	{
		case ADD_GROUP:
			purple_debug_info("mrim","[%s] ADD_GROUP\n", __func__);
			mg_add(0, mpq->add_group.name, id, mrim);
			break;
		case ADD_BUDDY:
			purple_debug_info("mrim","[%s]ADD_BUDDY\n", __func__);
			if (mpq->add_buddy.group_exsist)
			{	// добовляли buddy
				PurpleBuddy *buddy = mpq->add_buddy.buddy;
				mrim_buddy *mb = buddy->proto_data;
				mb->id = id;
				if (is_valid_email(buddy->name))
					send_package_authorize(mrim, buddy->name, (gchar *)(mrim->username));
			}
			else
			{	// добавляли в не существующую группу
				mg_add(0, mpq->add_buddy.group->name, id, mrim);
				mrim_add_buddy(mrim->gc, mpq->add_buddy.buddy, mpq->add_buddy.group); // TODO проверить
			}
			break;
		case MOVE_BUDDY:
			purple_debug_info("mrim","[%s] MOVE_BUDDY\n", __func__);
			// добавили группу. теперь перемещаем контакт
			mg_add(0, mpq->move_buddy.new_group, id, mrim);
			mrim_move_buddy(mrim->gc, mpq->move_buddy.buddy_name, NULL, mpq->move_buddy.new_group); // TODO проверить
			break;
		default:
			purple_debug_info("mrim","[%s] UNKNOWN mpq->type <%i>\n", __func__, mpq->type);
			break;
	}
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}

void mrim_modify_contact_ack(mrim_data *mrim ,package *pack)
{
	purple_debug_info("mrim","[%s] seq=<%u>\n",__func__, pack->header->seq);
	guint32 group_id; // нужен в case-е
	guint32 status = read_UL(pack);
	if (status != CONTACT_OPER_SUCCESS)
		print_cl_status(status);
	g_return_if_fail(status == CONTACT_OPER_SUCCESS);

	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
	if (mpq == NULL)
			purple_notify_warning(_mrim_plugin, "Работа с контакт-листом завершилась ошибкой!", "Работа с контакт-листом завершилась ошибкой!", "Такая операция не осуществлялась? (mpq == NUL)");
	g_return_if_fail(mpq != NULL);

	switch (mpq->type)
	{
		case MOVE_BUDDY:
			purple_debug_info("mrim","[%s] MOVE_BUDDY\n", __func__);
			break;
		case REMOVE_GROUP:
			purple_debug_info("mrim","[%s] REMOVE_GROUP\n", __func__);
			g_hash_table_remove(mrim->mg, GUINT_TO_POINTER(mpq->remove_group.group_id));
			break;
		case RENAME_GROUP:
			purple_debug_info("mrim","[%s] RENAME_GROUP\n", __func__);
			break;
		case REMOVE_BUDDY:
			purple_debug_info("mrim","[%s] REMOVE_BUDDY\n", __func__);
			// TODO remove buddy?
			free_buddy(mpq->remove_buddy.buddy);
			break;
		case MODIFY_BUDDY:
			purple_debug_info("mrim","[%s] MODIFY_BUDDY\n", __func__);
			PurpleBuddy *buddy = mpq->modify_buddy.buddy;
			if (buddy)
			{
				mrim_buddy *mb = buddy->proto_data;
				if (mb)
				{
					if (! (mb->phones))
					{
						mb->phones = g_new0(char *, 4);
					}
					if (mb->phones && mb->phones[0])
						purple_prpl_got_user_status(mrim->account, mb->addr, MRIM_STATUS_ID_MOBILE, NULL);
					else
						purple_prpl_got_user_status_deactive(mrim->account, mb->addr, MRIM_STATUS_ID_MOBILE);
				}
			}
			break;
		case SMS:
			purple_debug_info("mrim","[%s] SMS\n", __func__);
			guint32 status = read_UL(pack);
			switch (status)
			{
				case MRIM_SMS_OK:
					purple_notify_info(_mrim_plugin, "SMS", "Смс-ка Успешно доставлена", "");
					break;
				case MRIM_SMS_SERVICE_UNAVAILABLE:
					purple_notify_warning(_mrim_plugin, "SMS", "Услуга доставки СМС недоступна", "");
					break;
				case MRIM_SMS_INVALID_PARAMS:
					purple_notify_info(_mrim_plugin, "SMS", "Неверные параметры", "");
					break;
				default:
					purple_notify_error(_mrim_plugin, "SMS", "Что-то произошло не так!", "");
					break;
			}
			break;

		default:
			purple_debug_info("mrim","[%s] UNKNOWN mpq->type <%i>\n", __func__, mpq->type);
			break;
	}
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}



void mrim_mpop_session(mrim_data *mrim ,package *pack)
{
	purple_debug_info("mrim","[%s] seq=<%u>\n",__func__, pack->header->seq);
	gchar *webkey = NULL;
	gchar *url = NULL;
	guint32 status = read_UL(pack);
	if (status == MRIM_GET_SESSION_SUCCESS)
		webkey = read_LPS(pack);

	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
	if (mpq == NULL)
		purple_notify_warning(_mrim_plugin, "Работа с контакт-листом завершилась ошибкой!", "Работа с контакт-листом завершилась ошибкой!", "Такая операция не осуществлялась? (mpq == NUL)");
	g_return_if_fail(mpq);
	switch (mpq->type)
	{
		case NEW_EMAIL:
		{
			purple_debug_info("mrim","[%s] NEW_EMAIL\n", __func__);
			if (webkey)
				url =  g_strdup_printf("http://win.mail.ru/cgi-bin/auth?Login=%s&agent=%s", mrim->username ,webkey);
			else
				url = "mail.ru";

			if (purple_account_get_check_mail(mrim->account))
				purple_notify_email(mrim->gc, mpq->new_email.subject, mpq->new_email.from, mrim->username, url, NULL, NULL);
			break;
		}
		case NEW_EMAILS:
			purple_debug_info("mrim","[%s]NEW_EMAILS\n", __func__);
			notify_emails(mrim->gc, webkey, mpq->new_emails.count);
			break;
		case OPEN_URL:
			purple_debug_info("mrim","[%s] OPEN_URL webkey=<%s>\n", __func__, webkey);
			gchar *url = g_strdup_printf(mpq->open_url.url, webkey);
			purple_notify_uri(_mrim_plugin, url);
			break;
		default:
			purple_debug_info("mrim","[%s] UNKNOWN mpq->type <%i>\n", __func__, mpq->type);
			break;
	}
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}

void mrim_anketa_info(mrim_data *mrim, package *pack)
{
	purple_debug_info("mrim","[%s] seq=<%u>\n",__func__, pack->header->seq);
	guint32 status = read_UL(pack);

	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
	if (mpq == NULL)
		purple_notify_warning(_mrim_plugin, "Работа с анкетой завершилась ошибкой!", "Работа с анкетой завершилась ошибкой!", "Такая операция не осуществлялась? (mpq == NUL)");
	g_return_if_fail(mpq);
	if (status != MRIM_ANKETA_INFO_STATUS_OK)
	{
		switch (status) {
			case MRIM_ANKETA_INFO_STATUS_NOUSER:
				purple_notify_warning(_mrim_plugin, "Работа с анкетой завершилась ошибкой!", "Работа с анкетой завершилась ошибкой!", "MRIM_ANKETA_INFO_STATUS_NOUSER");
				break;
			case MRIM_ANKETA_INFO_STATUS_DBERR:
				purple_notify_warning(_mrim_plugin, "Работа с анкетой завершилась ошибкой!", "Работа с анкетой завершилась ошибкой!", "MRIM_ANKETA_INFO_STATUS_DBERR");
				break;
			case MRIM_ANKETA_INFO_STATUS_RATELIMERR:
				purple_notify_warning(_mrim_plugin, "Работа с анкетой завершилась ошибкой!", "Работа с анкетой завершилась ошибкой!", "MRIM_ANKETA_INFO_STATUS_RATELIMERR");
				break;
			default:
				purple_notify_warning(_mrim_plugin, "Работа с анкетой завершилась ошибкой!", "Работа с анкетой завершилась ошибкой!", "unknown error");
				break;
		}
		g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
		return;
	}


	switch (mpq->type)
	{
		case ANKETA_INFO:
		{
			purple_debug_info("mrim","[%s] ANKETA_INFO\n", __func__);
			gchar *param=NULL, *value=NULL;
			PurpleNotifyUserInfo *info = purple_notify_user_info_new();
			guint32 fields_num = read_UL(pack);
			read_UL(pack); // max_rows
			read_UL(pack); // DATE(unix)
			gchar *mas[fields_num][2];
			for(int i=0 ; i < fields_num ; i++)
				mas[i][0] = read_LPS(pack);
			for(int i=0 ; i < fields_num ; i++)
				mas[i][1] = read_LPS(pack);

			for(int i=0 ; i < fields_num ; i++)
			{
				if (! mas[i][0])
					continue;
				if (! mas[i][1])
					continue;

				if (strcmp(mas[i][0], "Sex")==0)
				{
					FREE(mas[i][0]);
					mas[i][0] = g_strdup("Пол");
					FREE(mas[i][1]);
					mas[i][1] = (mas[i][1] == "1")  ?  g_strdup("Мужской") : g_strdup("Женский");
				}
				if (strcmp(mas[i][0], "Zodiac")==0)
				{
					FREE(mas[i][0]);
					mas[i][0] = g_strdup("Зодиак");

					value = g_strdup(zodiak[ atoi(mas[i][1])-1 ]);
					FREE(mas[i][1]);
					mas[i][1] = value;
				}
				if (strcmp(mas[i][0], "City_id")==0)
					continue;
				if (strcmp(mas[i][0], "Country_id")==0)
					continue;
				if (strcmp(mas[i][0], "mrim_status")==0)
					continue;
				if (strcmp(mas[i][0], "BMonth")==0)
					continue;
				if (strcmp(mas[i][0], "BDay")==0)
					continue;
				if (strcmp(mas[i][0], "Username")==0)
					continue;
				if (strcmp(mas[i][0], "Domain")==0)
					continue;

				purple_notify_user_info_add_pair(info, mas[i][0], mas[i][1]);
			}

			for(int i=0 ; i < fields_num ; i++)
			{
				FREE(mas[i][0])
				FREE(mas[i][1])
			}

			purple_notify_userinfo(mrim->gc,        // connection the buddy info came through
			                       mpq->anketa_info.username,  // buddy's username
			                       info,      // body
			                       NULL,      // callback called when dialog closed
			                       NULL);     // userdata for callback
			break;

		}
		case SEARCH:
		{
			break;
		}
		default:
			purple_debug_info("mrim","[%s] UNKNOWN mpq->type <%i>\n", __func__, mpq->type);
			break;
	}
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}

void pq_free_element(gpointer data)
{// TODO
	purple_debug_info("mrim","%s\n",__func__);
	mrim_pq *mpq = (mrim_pq *)data;
	g_return_if_fail(mpq != NULL);
	switch(mpq->type)
	{
		case ADD_BUDDY: break;
		case ADD_GROUP: break;
		case RENAME_GROUP: break;
		case REMOVE_BUDDY: break;
		case REMOVE_GROUP:
			FREE(mpq->remove_group.group_name)
			break;
		case MOVE_BUDDY: break;
		case MESSAGE:
			FREE(mpq->message.to);
			FREE(mpq->message.message);
			break;
		case ANKETA_INFO: break;
		case SMS:
			//FREE(mpq->sms.phone)
			//FREE(mpq->sms.message)
			break;
		case MODIFY_BUDDY:
			break;
	}
	FREE(mpq);
}

void mg_free_element(gpointer data)
{// TODO
	mrim_group *mg = (mrim_group *)data;
	g_return_if_fail(mg != NULL);
	if (mg)
	{
		//FREE(mg->name)
		FREE(mg)
	}
}

static void print_cl_status(guint32 status)
{
	gchar *mes = NULL;
	switch (status)
	{
		case CONTACT_OPER_ERROR: mes = "Предоставленные данные были некорректны"; break;
		case CONTACT_OPER_INTERR: mes = "При обработке запроса произошла внутренняя ошибка"; break;
		case CONTACT_OPER_NO_SUCH_USER: mes = "Добовляемого пользователя не существует в системе"; break;
		case CONTACT_OPER_INVALID_INFO: mes = "Некорректное имя пользователя"; break;
		case CONTACT_OPER_USER_EXISTS: mes = "Контакт/группа не может быть добавленна"; break;
		case CONTACT_OPER_GROUP_LIMIT: mes = "Превышено максимальное количество групп"; break;
	}
	if (status != CONTACT_OPER_SUCCESS)
	{
		purple_notify_warning(_mrim_plugin, "Работа с контакт-листом завершилась ошибкой!", "Работа с контакт-листом завершилась ошибкой!", mes);
		return;
	}
}

void send_package_authorize(mrim_data *mrim, gchar *to, gchar *who) // TODO text // TODO who не нужне, т.к. есть mrim->username
{
	purple_debug_info("mrim","[%s]\n",__func__);
	(mrim->seq)++;
	// запрос авторизации
	gchar *text = "Здравствуйте. Пожалуйста, добавьте меня в список ваших контактов.";
	gchar *ctext = g_convert(text, -1, "CP1251", "UTF8", NULL, NULL, NULL);
	//gchar *ctext = g_convert(text, -1, "UTF-16LE" , "UTF8", NULL, NULL, NULL);
	gchar *cwho =  g_convert(who, -1, "CP1251" , "UTF8", NULL, NULL, NULL);

	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
  	add_ul(MESSAGE_FLAG_AUTHORIZE | MESSAGE_FLAG_NORECV, pack); //add_ul(MESSAGE_FLAG_AUTHORIZE |  0x00080000, pack);
  	add_LPS(to, pack);
  	add_base64(pack, FALSE, "uss", 2, cwho, ctext);
  	add_ul(0, pack);
  	send_package(pack, mrim);
}


//////
gchar *mrim_phones_to_string(gchar **phones)
{
	if (!phones)
		return NULL;
	// TODO mem leaks
	gchar *string = ""; // result string
	gchar *phone = *phones; //curent phone
	gchar *cl_phone = NULL;
	while (phone)
	{
		purple_debug_info("mrim","[%s] <%s>\n",__func__, *phones);
		++phones;
		cl_phone = clear_phone(phone); // TODO mem leaks
		if (cl_phone)
		{
			string = g_strconcat(string, cl_phone, NULL);
			if (*phones)
				string = g_strconcat(string, ",", NULL);
		}
		phone = *phones;
	}
	purple_debug_info("mrim","[%s] <%s>\n", __func__, string);
	return string;
}


void mrim_pkt_modify_buddy(mrim_data *mrim, PurpleBuddy *buddy, guint32 seq)
{
	g_return_if_fail(mrim);
	g_return_if_fail(buddy);
	g_return_if_fail(buddy->proto_data);
	mrim_buddy *mb = buddy->proto_data;
	gboolean mobile = (mb->flags & CONTACT_FLAG_PHONE);
	// Send package
	int g_count = g_hash_table_size(mrim->mg);
	package *pack = new_package(seq, MRIM_CS_MODIFY_CONTACT);
	add_ul(mb->id ,pack); // id
	add_ul(mb->flags,pack);  // флаги
	add_ul(mobile ? MRIM_PHONE_GROUP_ID : mb->group_id, pack);
	add_LPS(mobile ? "phone" : mb->addr, pack);
	add_LPS(mb->alias, pack);
	add_LPS(mrim_phones_to_string(mb->phones), pack);
	send_package(pack, mrim);

}
void mrim_pkt_modify_group(mrim_data *mrim, guint32 group_id, gchar *group_name, guint32 flags)
{
	g_return_if_fail(mrim);
	g_return_if_fail(group_name);
	// Send package
	package *pack = new_package(mrim->seq, MRIM_CS_MODIFY_CONTACT);
	add_ul(group_id, pack);
	add_ul(flags | CONTACT_FLAG_REMOVED, pack);
	add_ul(0, pack);
	add_LPS(group_name, pack); // новое имя
	add_ul(0,pack);
	add_ul(0,pack);

}
void mrim_pkt_add_group(mrim_data *mrim, gchar *group_name, guint32 seq)
{
	guint32 groups_count = g_hash_table_size(mrim->mg);
	package *pack = new_package(seq, MRIM_CS_ADD_CONTACT);
	add_ul(CONTACT_FLAG_GROUP | (groups_count << 24), pack);
	add_ul(0, pack);
	add_LPS(group_name, pack); // кодировка?
	add_ul(0, pack);
	add_ul(0, pack);
	add_ul(0, pack);
	send_package(pack, mrim);
}
