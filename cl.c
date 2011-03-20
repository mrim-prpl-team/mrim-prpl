/**   cl.c of mrim-prpl project.
* Contact List management routines.
* Committed by Reslayer@mail.ru aka Reslayer.
*/
#include "cl.h"
/******************************************
 *        *Loading contacts list.*
 ******************************************/
static mrim_buddy *new_mrim_buddy(package *pack, gchar *mask);

static void cl_skeep(gchar *mask, package *pack)
{
	while (*mask)
		switch (*mask++)
		{
			case 's': g_free(read_rawLPS(pack)); break;
			case 'u': read_UL(pack); break;
			case 'z': read_Z(pack); break;
		}
}

void mrim_cl_load(PurpleConnection *gc, mrim_data *mrim, package *pack)
{	
	PurpleAccount *account = purple_connection_get_account(gc);
	guint32 g_number = read_UL(pack);// Groups count
	gchar *g_mask = read_LPS(pack); // Group mask
	gchar *c_mask = read_LPS(pack); // Contact mask

	purple_debug_info("mrim", "Group number <%u>, Group mask <%s>, Contact mask <%s>\n", g_number, g_mask, c_mask);

	/* Phone group */
	mg_add(0, "phone", MRIM_PHONE_GROUP_ID, mrim);

	/** reading groups **/
	guint32 i = 0;
	for (i=0; i < g_number ; i++)
	{
		guint32 flags = read_UL(pack);//  & 0x00FFFFFF;
		gchar *name = read_LPS(pack); // groups (UTF16)
		mg_add(flags, name, i, mrim);
		FREE(name)
		cl_skeep(g_mask + 2, pack);
	}

	/** reading contacts **/
	guint32 num = MRIM_MAX_GROUPS;
	while ( TRUE ) 
	{
		if (pack->cur >= pack->buf + pack->len)
			break; // To avoid trashing debug output.
		mrim_buddy *mb = new_mrim_buddy(pack, c_mask);
		if (!mb)
			break;
		mb->id = num++;
		if (mb->flags & CONTACT_FLAG_REMOVED)
		{
			purple_debug_info("mrim", "CONTACT: group <%i>  E-MAIL <%s> NICK <%s> id <%i> status <0x%X> flags <0x%X> REMOVED\n", mb->group_id, mb->addr, mb->alias, mb->id, (int)mb->status.code, mb->flags );
			continue;
		}
		else
			purple_debug_info("mrim", "CONTACT: group <%i>  E-MAIL <%s> NICK <%s> id <%i> status <0x%X> flags <0x%X>\n", mb->group_id, mb->addr, mb->alias, mb->id, (int)mb->status.code, mb->flags );


		PurpleGroup *group = get_mrim_group_by_id(mrim, mb->group_id);
		if (mb->type == CHAT)
		{
			// TODO assign id
			purple_debug_info("mrim", "[%s] <%s> is CHAT\n", __func__, mb->addr);
			PurpleChat *old_pc = purple_blist_find_chat(account, mb->addr);
			if (! old_pc)
				;
			else
			{
				GHashTable *defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
				PurpleChat *pc = purple_chat_new(account, mb->addr, defaults);
				//purple_conv_chat_set_id(,1);
				purple_blist_add_chat(pc, group, NULL);
				//purple_blist_alias_chat(pc, mb->alias);
				purple_debug_info("mrim", "[%s] <%s> !exsist\n", __func__, mb->addr);
			}
			continue;
		}


		PurpleBuddy *buddy = NULL;
		if (group)
		{ /*************/
			/* ADD BUDDY */
			/*************/
			// 1) If met before -- attach,
			//    otherwise create new one.
			PurpleBuddy *old_buddy = purple_find_buddy(account, mb->addr);
			if (old_buddy)
			{
				purple_debug_info("mrim", "Buddy <%s> already exsists!\n", old_buddy->name);
				// TODO Move to appropriate group.
				buddy = old_buddy;
			}
			else
			{
				purple_debug_info("mrim", "Never met this contact!\n");
				buddy = purple_buddy_new(gc->account, mb->addr, mb->alias);
				purple_blist_add_buddy(buddy, NULL/*contact*/, group, NULL/*node*/);
			}

			purple_buddy_set_protocol_data(buddy, mb);
			mb->buddy = buddy;
			if (!(mb->phones))
				mb->phones = g_new0(char *, 4);

			// Alias.
			purple_blist_alias_buddy(buddy, mb->alias);
			// Status.
			set_user_status_by_mb(mrim, mb);
			// Userpics.
			if (purple_account_get_bool(account, "fetch_avatar", FALSE))
				mrim_fetch_avatar(buddy);// TODO Where should we fetch userpics from? // TODO PQ
		}
	}

	/* Purge all obsolete buddies. */
	GSList *buddies = purple_find_buddies(gc->account, NULL);
	GSList *first = buddies;
	while (buddies)
	{
		PurpleBuddy *buddy = (PurpleBuddy*) (buddies->data);
		if (buddy)
		{
			if (!(buddy->proto_data))
			{
				purple_debug_info("mrim", "[%s] purge <%s>\n", __func__, buddy->name);
				purple_blist_remove_buddy(buddy);
			}
		}
		buddies = g_slist_next(buddies);
	}
	g_slist_free(first);

	//purple_blist_remove_chat

	purple_blist_show();
	purple_debug_info("mrim","[%s]: Contact list loaded!\n", __func__);
	FREE(g_mask);
	FREE(c_mask);
}

static mrim_buddy *new_mrim_buddy(package *pack, gchar *mask)
{
	mrim_buddy *mb = g_new(mrim_buddy, 1);
	// Read fields
	mb->flags = read_UL(pack); // Flag.
	guint32 gr_id = mb->group_id = read_UL(pack); // Group ID
	mb->addr = read_LPS(pack); // Buddy address (UTF16LE)
	mb->alias = read_LPS(pack); // Nick (UTF16LE)
	mb->s_flags= read_UL(pack); // Server flag (not authorized)
	guint32 status = read_UL(pack); // Status.
	gchar *phones = read_LPS(pack); // Phone number.

	gchar *status_uri		= read_LPS(pack);
	gchar *status_title	= read_LPS(pack);
	gchar *status_desc	= read_LPS(pack);
	read_UL(pack);
	mb->user_agent		= read_LPS(pack);
	
	// sssusuuusssss
	cl_skeep(mask+12, pack);
	
	mb->status.display_string = NULL;
	mb->status.uri = NULL;
	mb->status.title = NULL;
	mb->status.desc = NULL;
	mb->status.purple_status = NULL;
	make_mrim_status(&mb->status, status, status_uri, status_title, status_desc);

	if (mb->flags & CONTACT_FLAG_MULTICHAT)
		mb->type = CHAT;
	else mb->type = BUDDY;
	//else if (mb->flags & ) // TODO
	if (gr_id > MRIM_MAX_GROUPS)
		mb->group_id = MRIM_DEFAULT_GROUP_ID;

	//parse phones
	mb->phones = g_new0(char *, 4);
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

	// Rename phone contacts.
	if ( mb && (mb->flags & CONTACT_FLAG_PHONE) )
	{
		purple_debug_info("mrim","[%s] rename phone buddy\n",__func__);
		FREE(mb->addr)
		mb->addr = g_strdup(mb->phones[0]);
		mb->authorized = TRUE;
		//mb->status = STATUS_ONLINE;
	}
/*	if (strcmp(mb->addr, "phone") == 0) // TODO Think of it.
	{
		purple_debug_info("mrim","[%s] rename phone buddy to %s\n",__func__, mb->phones[0]);
		g_free(mb->addr);
		mb->addr = g_strdup(mb->phones[0]);
		mb->status = STATUS_ONLINE;
		mb->flags |= CONTACT_FLAG_PHONE;
	}
*/
	if (! mb->authorized)
		make_mrim_status(&mb->status, STATUS_OFFLINE, "", "", "");

	if (mb->addr == NULL)
		return NULL;
	else
		return mb;
}

/******************************************
 *               *Groups.*
 ******************************************/
// Group creation request to server.
void mrim_add_group(mrim_data *mrim, char *name)
{
	purple_debug_info("mrim","[%s] group_name=<%s>\n",__func__, name);

	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = ADD_GROUP;
	mpq->seq = mrim->seq;
	mpq->add_group.name = name;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mrim->seq), mpq);

	mrim_pkt_add_group(mrim, name, mpq->seq);
	// TODO Should we create purple group?? Or it is auto-handled with libpurple?
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

	//int g_count = g_hash_table_size (mrim->mg);
	guint32 group_id = get_mrim_group_id_by_name(mrim, (gchar *)old_name);
	if (MRIM_NO_GROUP == group_id)
	{
		purple_notify_warning(_mrim_plugin, _("Encountered an error while working on contact list!"), _("Encountered an error while working on contact list!"), _("Group not found."));
		return;
	}
	mrim_group *mg = g_hash_table_lookup(mrim->mg, GUINT_TO_POINTER(group_id));
	guint32 flags = CONTACT_FLAG_GROUP;
	if (mg)
	{
		flags = mg->flag;
		// Change mrim->mg
		mg->name = group->name;
		mg->gr = group;
	}

	mrim_pkt_modify_group(mrim, group_id, group->name, flags);
	// TODO Should we regroup buddies?
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

	mrim_pkt_modify_group(mrim, group_id, group->name, flags | CONTACT_FLAG_REMOVED | (8<<24) |CONTACT_FLAG_SHADOW);
}


// MG
void mg_add(guint32 flags, gchar *name, guint id, mrim_data *mrim)
{
	purple_debug_info("mrim", "[%s] Group id=<%u> flag=<%x> <%s>\n", __func__, id, flags, name);
	if (flags & CONTACT_FLAG_REMOVED)
		purple_debug_info("mrim", "[%s] Group <%s> REMOVED\n", __func__, name);
	if (flags & CONTACT_FLAG_SHADOW)
	{
		purple_debug_info("mrim", "[%s] Group <%s> SHADOW. skip it\n", __func__ ,name);
		return;
	}
	mrim_group *mg = g_new0(mrim_group, 1);
	mg->flag = flags;
	mg->name = g_strdup(name);
	mg->id = id;
	PurpleGroup *gr = purple_find_group(mg->name);
	if (gr == NULL)
	{
		gr = purple_group_new(mg->name);
		purple_blist_add_group(gr, NULL);
	}
	mg->gr = gr;
	g_hash_table_insert(mrim->mg, GUINT_TO_POINTER(id), mg);
}
// Look for group by its id:
PurpleGroup *get_mrim_group_by_id(mrim_data *mrim, guint32 id)
{
//	purple_debug_info("mrim","[%s]\n",__func__);
	mrim_group *mg =  g_hash_table_lookup(mrim->mg, GUINT_TO_POINTER(id));
	g_return_val_if_fail(mg != NULL, NULL);
	if (mg->gr)
		purple_debug_info("mrim", "Found grp %s, ID <%u> \n", mg->gr->name, id);
	else
		purple_debug_info("mrim", "Not found group by ID <%u>\n", id);
	return mg->gr;
}
// Look for group by its name:
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
 *             *Contacts.*
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

	gchar *normalized_name = mrim_normalize(gc->account, buddy->name);
	FREE(buddy->name)
	buddy->name = normalized_name;


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
	
	if (!is_valid_buddy_name(buddy->name))
	{
		gchar *buf;
		buf = g_strdup_printf(_("Unable to add the buddy \"%s\" because the username is invalid.  Usernames must be a valid email address(in mail.ru bk.ru list.ru corp.mail.ru inbox.ru domains), or valid phone number (start with + and contain only numbers, spaces and \'-\'."), buddy->name);
		purple_notify_error(gc, NULL, _("Unable to Add"), buf);
		g_free(buf);
		/* Remove from local list */
		purple_blist_remove_buddy(buddy);
		return;
	}

	mrim_buddy *mb;
	mrim_data *mrim = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBuddy *old_buddy = purple_find_buddy(account, buddy->name);

	/* 2) If met such contact -- update. Otherwise -- add.  */
	if (old_buddy != NULL  && old_buddy != buddy)
	{
		purple_debug_info("mrim","Buddy <%s> already exsists!\n", old_buddy->name);
		//purple_buddy_destroy(buddy);
		purple_blist_remove_buddy(buddy);
		buddy = old_buddy;
		mb = (mrim_buddy*)(buddy->proto_data);

		if (mb)
		{
			purple_debug_info("mrim","[%s] mb exsists\n",__func__);
			mb->buddy = buddy;
			purple_blist_alias_buddy(buddy, mb->alias);
			set_user_status_by_mb(mrim, mb);
		}
	}
	else
	{
		purple_debug_info("mrim","Never met such contact!\n");
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

			purple_blist_add_buddy(buddy, NULL, group, NULL); // Add only if the group exists.
			clean_string(buddy->name);
			if (is_valid_email(buddy->name))
			{
				purple_debug_info("mrim","[%s] it is email\n",__func__);
				mpq->add_buddy.authorized = FALSE;
				mb->addr = g_strdup(buddy->name);
				mb->authorized = FALSE;
				mb->group_id = group_id;
				mb->flags = 0;
				mb->user_agent = NULL;

				// TODO use send_package_authorize
				gchar *text = _("Hello. Add me to your buddies please.");
				gchar *ctext = g_convert(text, -1, "CP1251" , "UTF8", NULL, NULL, NULL);
				gchar *who = (buddy->alias)?(buddy->alias):(buddy->name);
				//gchar *cwho = g_convert(who, -1, "CP1251" , "UTF8", NULL, NULL, NULL);

				package *pack = new_package(mpq->seq, MRIM_CS_ADD_CONTACT);
				add_ul(0, pack); // Just add.
				add_ul(group_id, pack);
				add_LPS(buddy->name, pack);
				add_LPS(who, pack); // Pseudonim (nickname/alias)
				add_ul(0, pack); // null lps - phones
				add_base64(pack, FALSE, "uss", 2, mrim->username, ctext); //TODO Auth message.
				add_ul(0, pack);
				send_package(pack, mrim);
			}
			else if (is_valid_phone(buddy->name))
			{	//TODO WTF? Merge these two almost identical blocks into a single function.
				// If we ever need auth for phone contacts...
				purple_debug_info("mrim","[%s] it is phone\n",__func__);
				mpq->add_buddy.authorized = TRUE;
				mb->phones[0] = g_strdup(buddy->name);
				mb->flags = CONTACT_FLAG_PHONE;
				mb->authorized = TRUE;
				mb->group_id = MRIM_PHONE_GROUP_ID;
				mb->addr = g_strdup("phone");
				make_mrim_status(&mb->status, STATUS_ONLINE, "", "", "");

				// TODO use send_package_authorize
				gchar *text = _("Hello. Add me to your buddies please.");
				gchar *ctext = g_convert(text, -1, "CP1251" , "UTF8", NULL, NULL, NULL);
				gchar *who = (buddy->alias)?(buddy->alias):(buddy->name);
				gchar *cwho = g_convert(who, -1, "CP1251" , "UTF8", NULL, NULL, NULL);

				package *pack = new_package(mpq->seq, MRIM_CS_ADD_CONTACT);
				add_ul(CONTACT_FLAG_PHONE, pack); // Just add.
				add_ul(MRIM_PHONE_GROUP_ID, pack);
				add_LPS(mb->addr, pack);
				add_LPS((buddy->alias)?(buddy->alias):NULL, pack); // Pseudonim (nickname/alias)
				add_LPS(mrim_phones_to_string(mb->phones), pack);
				add_base64(pack, FALSE, "uss", 2, cwho, ctext); //TODO Auth message.
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
	g_return_if_fail(mb != NULL); // TODO Put buddy back to buddy list??
	
	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = REMOVE_BUDDY;
	mpq->seq = mrim->seq;
	mpq->remove_buddy.buddy = buddy;
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	mb->flags |= CONTACT_FLAG_REMOVED;
	mrim_pkt_modify_buddy(mrim, buddy, mpq->seq);
	purple_debug_info("mrim", "[%s]removing %s from %s's buddy list. id=<%u> group_id=<%u>\n",__func__,buddy->name, gc->account->username, mb->id, mb->group_id);
}


void free_buddy_proto_data(PurpleBuddy *buddy)
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

	guint32 group_id = get_mrim_group_id_by_name(mrim, (gchar *) new_group);
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
 *               *Userpics.*
 ******************************************/
void mrim_fetch_avatar(PurpleBuddy *buddy)
{
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(buddy->name != NULL);
	purple_debug_info("mrim","[%s] <%s>\n",__func__, buddy->name);
	if (! is_valid_email(buddy->name))
		return;
	if ((!buddy->icon) && buddy->name)
	{
		// TODO Load only missing userpics.
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

void mrim_avatar_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
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
 *               *Authorization.*
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
{	// Sending nothing, just do mem free.
	purple_debug_info("mrim","[%s]\n", __func__);
	auth_data *a_data = (auth_data *) va_data;
	g_free(a_data->from);
	g_free(a_data);
}



/******************************************
 *               *Queues.*
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
		purple_notify_warning(_mrim_plugin, _("Encountered an error while working on contact list!"), _("Encountered an error while working on contact list!"), _("Did you ever do this operation? (mpq == NULL)"));
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
			{	// Added buddy
				PurpleBuddy *buddy = mpq->add_buddy.buddy;
				mrim_buddy *mb = buddy->proto_data;
				mb->id = id;
				// TODO bug 22
				if (is_valid_email(buddy->name))
					send_package_authorize(mrim, buddy->name, (gchar *)(mrim->username));
			}
			else
			{	// Added to not existing group.
				mg_add(0, mpq->add_buddy.group->name, id, mrim);
				mrim_add_buddy(mrim->gc, mpq->add_buddy.buddy, mpq->add_buddy.group); // TODO Recheck.
			}
			break;
		case MOVE_BUDDY:
			purple_debug_info("mrim","[%s] MOVE_BUDDY\n", __func__);
			// Added group, move contact.
			mg_add(0, mpq->move_buddy.new_group, id, mrim);
			mrim_move_buddy(mrim->gc, mpq->move_buddy.buddy_name, NULL, mpq->move_buddy.new_group); // TODO Recheck.
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
	//guint32 group_id; // нужен в case-е
	guint32 status = read_UL(pack);
	if (status != CONTACT_OPER_SUCCESS)
		print_cl_status(status);
	g_return_if_fail(status == CONTACT_OPER_SUCCESS);

	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
	if (mpq == NULL)
			purple_notify_warning(_mrim_plugin, _("Encountered an error while working on contact list!"), _("Encountered an error while working on contact list!"), _("Did you ever do this operation? (mpq == NULL)"));
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
					purple_notify_info(_mrim_plugin, _("SMS"), _("SMS delivered successfully."), "");
					break;
				case MRIM_SMS_SERVICE_UNAVAILABLE:
					purple_notify_warning(_mrim_plugin, _("SMS"), _("SMS service is not available."), "");
					break;
				case MRIM_SMS_INVALID_PARAMS:
					purple_notify_info(_mrim_plugin, _("SMS"), _("Wrong SMS parameters."), "");
					break;
				default:
					purple_notify_error(_mrim_plugin, _("SMS"), _("Something went wrong!"), "");
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
		purple_notify_warning(_mrim_plugin, _("Encountered an error while working on contact list!"), _("Encountered an error while working on contact list!"), _("Did you ever do this operation? (mpq == NULL)"));
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
			purple_debug_info("mrim","[%s] NEW_EMAILS\n", __func__);
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
	// TODO: Define string constants for the most used messages (unify speech).
	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
	if (mpq == NULL)
		purple_notify_warning(_mrim_plugin, _("Encountered an error while working on user details!"), _("Encountered an error while working on user details!"), _("Did you ever do this operation? (mpq == NULL)"));
	g_return_if_fail(mpq);
	if (status != MRIM_ANKETA_INFO_STATUS_OK)
	{
		switch (status) {
			case MRIM_ANKETA_INFO_STATUS_NOUSER:
				purple_notify_warning(_mrim_plugin, _("Encountered an error while working on user details!"), _("Encountered an error while working on user details!"), _("User not found."));
				break;
			case MRIM_ANKETA_INFO_STATUS_DBERR:
				purple_notify_warning(_mrim_plugin, _("Encountered an error while working on user details!"), _("Encountered an error while working on user details!"), _("DBERR error. Please try later."));
				break;
			case MRIM_ANKETA_INFO_STATUS_RATELIMERR:
				purple_notify_warning(_mrim_plugin, _("Encountered an error while working on user details!"), _("Encountered an error while working on user details!"), _("MRIM_ANKETA_INFO_STATUS_RATELIMERR"));
				break;
			default:
				purple_notify_warning(_mrim_plugin, _("Encountered an error while working on user details!"), _("Encountered an error while working on user details!"), _("unknown error"));
				break;
		}
		g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
		return;
	}

	/* Processing user details. */
	purple_debug_info("mrim","[%s] PARSE DATA\n", __func__);
	gchar *value=NULL;

	guint32 fields_num = read_UL(pack);
	guint32 max_rows = read_UL(pack);
	guint32 real_rows = 0;
	gchar *header[fields_num+1];
	gchar *mas[max_rows][fields_num+1];
	gboolean skip[fields_num+1]; // Fields to skip (CountryID, BDay...)
	guint32 username_index=-1,domain_index=-1;

	guint32 date = read_UL(pack); // DATE(unix)

	// Reading columns headers. Storing username и domain columns indexes.
	header[0] = g_strdup( _("email") );
	skip[0] = FALSE;
	for(guint32 j=1 ; j <= fields_num ; j++)
	{
		skip[j]=FALSE;
		header[j] = read_LPS(pack);
		if (strcmp(header[j], "Username")==0)	{	skip[j]=TRUE;	username_index=j;	purple_debug_info("mrim","[%s] username_index %u\n", __func__,username_index);continue; }
		if (strcmp(header[j], "Domain")==0)		{	skip[j]=TRUE;	domain_index=j;		purple_debug_info("mrim","[%s] domain_index %u\n", __func__,domain_index);continue; }
		if (strcmp(header[j], "City_id")==0)	{	skip[j]=TRUE;	continue; }
		if (strcmp(header[j], "Country_id")==0)	{	skip[j]=TRUE;	continue; }
		if (strcmp(header[j], "mrim_status")==0){	skip[j]=TRUE;	continue; }
		if (strcmp(header[j], "BMonth")==0)		{	skip[j]=TRUE;	continue; }
		if (strcmp(header[j], "BDay")==0)		{	skip[j]=TRUE;	continue; }
		if (strcmp(header[j], "Location")==0)	{	skip[j]=TRUE;	continue; }
		
		// i18n for userinfo headers:
		gchar *header_name = g_strdup ( header[j] );
		FREE(header[j]);
		for ( int headerIndex = 0; headerIndex < info_header_size; headerIndex ++ )
		{
			if ( strcmp ( header_name, info_header [headerIndex] ) == 0)
			{
				header_name = g_strdup( _(info_header_i18n[headerIndex]) );
				break;
			}
		};
		header[j] = g_strdup( header_name );
		FREE(header_name);
	}

	for (guint32 i = 0; i < max_rows; i++)
	{
		for(guint32 j=1 ; j <= fields_num ; j++)
		{
			 
			mas[i][j] = read_LPS(pack);
		}

		real_rows = i+1; // TODO

		if (domain_index != -1 && username_index != -1)
			mas[i][0] = g_strdup_printf("%s@%s", mas[i][username_index],  mas[i][domain_index]);
		else
			mas[i][0] = (gchar *)g_new0(gchar ,1); // Void string.

		if (pack->buf + pack->len <= pack->cur)
					break;
	}

	purple_debug_info("mrim","[%s] REAL_ROWS =<%u/%u>!\n",__func__, real_rows,max_rows);

	for(guint32 j=0 ; j <= fields_num ; j++)
	{
		if (skip[j]) { continue; }
		else {
			// Parse values for several choices.
			if (strcmp(header[j], "Sex")==0)
			{
				FREE(header[j]);
				header[j] = g_strdup( _("Sex") );

				for (guint32 i=0; i<real_rows; i++)
					if (! mas[i][j])
						continue;
					else
					{	// Sex description:
						value = (atoi(mas[i][j]) == 1)  ?  g_strdup( _("Male") ) : g_strdup( _("Female") );
						FREE(mas[i][j]);
						mas[i][j] = value;
					}
			} else if (strcmp(header[j], "Zodiac")==0)
			{
				FREE(header[j]);
				header[j] = g_strdup( _("Zodiac") );

				for (guint32 i=0; i<real_rows; i++)
					if (! mas[i][j])
						continue;
					else
					{	// Zodiac description:
						value = g_strdup ( mas[i][j] );
						FREE(mas[i][j]);
						mas[i][j] = g_strdup( _(zodiac[atoi(value)-1]) );
						//mas[i][j] = g_strdup ( gettext (zodiac[atoi(value)-1]) );
						FREE(value);
					}
			}
		}
	}

	switch (mpq->type)
	{
		case ANKETA_INFO:
		{
			purple_debug_info("mrim","[%s] ANKETA_INFO\n", __func__);
			PurpleNotifyUserInfo *info = purple_notify_user_info_new();
			for(guint32 j=0 ; j <= fields_num ; j++)
				if (!skip[j])
					purple_notify_user_info_add_pair(info, header[j], mas[0][j]);
			gchar *mb_email = g_strdup_printf("%s@%s", mas[0][username_index],  mas[0][domain_index]);
			PurpleBuddy *buddy = purple_find_buddy(mrim->account, mb_email);
			if (buddy)
			{
				mrim_buddy *mb = buddy->proto_data;
				if (mb && mb->user_agent)
				{
					purple_notify_user_info_add_pair(info, _("User agent"), _(mrim_get_ua_alias(mb->user_agent)) );
				} else
				{
					purple_notify_user_info_add_pair(info, _("User agent"), _("Hidden") );
				}
			}
			purple_notify_userinfo(mrim->gc,        // connection the buddy info came through
				mpq->anketa_info.username,  // buddy's username
				info,      // body
				NULL,      // callback called when dialog closed // TODO Maybe mem free needed?
				NULL);     // userdata for callback
			break;

		}
		case SEARCH:
		{
			PurpleNotifySearchResults *results;
			PurpleNotifySearchColumn *column;
			GList *row;

			results = purple_notify_searchresults_new();
			if (results == NULL)
			{
				purple_debug_info("mrim","[%s] results == NULL!\n",__func__);
				break;
			}
			for(guint32 j=0 ; j <= fields_num ; j++)
				if (!skip[j])
				{
					purple_debug_info("mrim","[%s] add <%s>\n",__func__,header[j]);
					column = purple_notify_searchresults_column_new(header[j]);
					purple_notify_searchresults_column_add(results, column);
				}
				else
				{
					purple_debug_info("mrim","[%s] skip <%s>\n",__func__,header[j]);
				}

	        //buttons: Add Contact, Close
	        purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_ADD, mrim_searchresults_add_buddy);

	        for(guint32 i=0 ; i < real_rows ; i++)
	        {
	        	row = NULL;

	        	for(guint32 j=0 ; j <= fields_num ; j++)
	        		if (!skip[j])
	        			row = g_list_append(row, g_strdup(mas[i][j])); // TODO mem leaks?

	        	purple_notify_searchresults_row_add(results, row);
	        }

	        purple_notify_searchresults(mrim->gc,
	                        NULL,
	                        _("Search results"), NULL, results,
	                        NULL, //PurpleNotifyCloseCallback // TODO Should we do a mem free???
	                        mrim);

			break;
		}
		default:
			purple_debug_info("mrim","[%s] UNKNOWN mpq->type <%i>\n", __func__, mpq->type);
			break;
	}

	for(guint32 i=0 ; i < real_rows ; i++)
		for(guint32 j=0 ; j <= fields_num ; j++)
			FREE(mas[i][j])
	for(guint32 j=0; j<=fields_num; j++)
		FREE(header[j])

	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}

void mrim_searchresults_add_buddy(PurpleConnection *gc, GList *row, void *user_data)
{
	mrim_data *mrim = user_data;
	purple_debug_info("mrim","%s", mrim->account->username);

    if (!purple_find_buddy(mrim->account, g_list_nth_data(row, 0)))
            purple_blist_request_add_buddy(mrim->account,  g_list_nth_data(row, 0), NULL, NULL); // TODO Propose alias automatically.
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
		case AVATAR:
			break;
		case SEARCH:
			break;
		case OPEN_URL:
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

void print_cl_status(guint32 status)
{
	gchar *mes = NULL;
	switch (status)
	{
		case CONTACT_OPER_ERROR: mes = _("Invalid data provided."); break;
		case CONTACT_OPER_INTERR: mes = _("Internal error encountered while processing request."); break;
		case CONTACT_OPER_NO_SUCH_USER: mes = _("No such user as you added."); break;
		case CONTACT_OPER_INVALID_INFO: mes = _("Invalid user name."); break;
		case CONTACT_OPER_USER_EXISTS: mes = _("Buddy/group cannot be added."); break;
		case CONTACT_OPER_GROUP_LIMIT: mes = _("Max groups allowed count exceedeed."); break;
	}
	if (status != CONTACT_OPER_SUCCESS)
	{	// TODO: String constants for the most used messages.
		purple_notify_warning(_mrim_plugin, _("Encountered an error while working on contact list!"), _("Encountered an error while working on contact list!"), mes);
		return;
	}
}

/******************************************
 *               *Packages.*
 ******************************************/
void send_package_authorize(mrim_data *mrim, gchar *to, gchar *who) // TODO text // TODO who is not needed for we got mrim->username
{
	purple_debug_info("mrim","[%s]\n",__func__);
	(mrim->seq)++;
	// Auth request.
	gchar *text = _("Hello. Add me to your buddies please.");
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
		++phones;
		cl_phone = clear_phone(phone); // TODO mem leaks
		if (cl_phone)
		{
			string = g_strconcat(string, cl_phone, NULL);
			if (*phones)
				string = g_strconcat(string, ",", NULL);
		}
		phone = *phones;
#ifdef DEBUG
		purple_debug_info("mrim","[%s] <%s>\n",__func__, string);
#endif
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
	//guint g_count = g_hash_table_size(mrim->mg); // TODO why not used?
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
	add_ul(flags, pack);
	add_ul(0, pack);
	add_LPS(group_name, pack); // New name.
	add_ul(0,pack);
	add_ul(0,pack);
	send_package(pack, mrim);
}
void mrim_pkt_add_group(mrim_data *mrim, gchar *group_name, guint32 seq)
{
	guint32 groups_count = g_hash_table_size(mrim->mg);
	groups_count -= 1; // phone group
	purple_debug_info("mrim", "[%s] groups_count=<%u>\n", __func__, groups_count);
	package *pack = new_package(seq, MRIM_CS_ADD_CONTACT);
	add_ul(CONTACT_FLAG_GROUP | (groups_count << 24), pack);
	add_ul(0, pack);
	add_LPS(group_name, pack); // Encoding???
	add_ul(0, pack);
	add_ul(0, pack);
	add_ul(0, pack);
	send_package(pack, mrim);
}

