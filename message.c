#include "mrim.h"
#include "message.h"
#include "package.h"
#include "cl.h"

guint32 atox(gchar *str)
{
	g_return_val_if_fail(str,0);
	purple_debug_info("mrim", "[%s] <%s>\n", __func__, str);
	guint32 res = 0;
	while (*str)
	{
		res *= 16;
		if (*str >= '0' && *str <='9')
			res += *str-'0';
		else if (*str >= 'A' && *str<='F')
			res += *str-'A'+0xA;
		else if (*str >= 'a' && *str<='f')
			res += *str-'a'+0xa;
		str++;
	}
	purple_debug_info("mrim", "[%s] <%x>\n", __func__, res);
	return res;
}

/******************************************
 *           *Offline messages.*
 ******************************************/
// Offline message reading...
void mrim_message_offline(PurpleConnection *gc, char* message)
{
	mrim_data *mrim = gc->proto_data;
	purple_debug_info("mrim", "parse offline message\n");
	if (!message)
		return;

	gchar* from = mrim_message_offline_get_attr("From:", message);
	gchar* date_str = mrim_message_offline_get_attr("Date:", message);
	gchar* charset = mrim_message_offline_get_attr("Charset:", message);
	gchar* msg = mrim_message_offline_get_attr("MSG", message);
	gchar* encoding = mrim_message_offline_get_attr("Content-Transfer-Encoding:", message);
	time_t date = mrim_str_to_time(date_str);
	gchar* flags = mrim_message_offline_get_attr("X-MRIM-Flags:", message);
	guint32 mrim_flags = atox(flags);
	gchar* correct_code = NULL;
	gchar *draft_message = NULL;



	if (mrim_flags & MESSAGE_FLAG_AUTHORIZE)
	{	// Auth request.
		purple_debug_info("mrim"," offline auth\n");
		/*guint32 decoded_len;
		decoded = purple_base64_decode(msg,  &decoded_len); // TODO Beware of signed!
		purple_debug("mrim","[%s] %s\n",__func__,mes);
*/
		auth_data *a_data = g_new0(auth_data, 1);
		a_data->from = g_strdup(from);
		a_data->seq = mrim->seq; // TODO wtf?
		a_data->mrim = mrim;
		gboolean is_in_blist = (purple_find_buddy(mrim->account,from) != NULL);
		purple_account_request_authorization(mrim->account, from, NULL, NULL, NULL, is_in_blist, mrim_authorization_yes, mrim_authorization_no, a_data);
	}
	else
	{
		if (encoding)
		{
			gchar* msg_decoded=NULL;
			gsize len_decoded=0;
			gsize len_correct=0;

			encoding = g_ascii_tolower( *g_strstrip(encoding) ); // TODO test
			if(encoding && g_strcmp0(encoding,"base64")==0)
			{
				msg_decoded = (gchar*) purple_base64_decode(msg, &len_decoded); // Allowed?
				len_correct = len_decoded;
				correct_code = g_memdup(msg_decoded,len_decoded+1);
				correct_code[len_decoded] = '\0';
				FREE(msg_decoded);
			}
		}


		if (correct_code)
			draft_message = strdup(correct_code); // TODO HTML tags
		else if(msg)
			draft_message = strdup(msg);
		else if(message)
			draft_message = strdup(message);

	#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >5
		gchar *correct_message = purple_markup_escape_text (draft_message, -1);
	#else
		gchar *correct_message = g_markup_escape_text(draft_message, -1);
	#endif
		serv_got_im(gc, from, correct_message, PURPLE_MESSAGE_RECV, date);
		FREE(correct_message);
	}

	FREE(correct_code);
	FREE(from);
	FREE(date_str);
	FREE(charset);
	FREE(msg);
	FREE(draft_message);
}

gchar* mrim_message_offline_get_attr(const gchar* attr,void* input)
{
    char* retVal = NULL;
	GRegex *regex;
	gboolean res;
	GMatchInfo *match_info;

    gchar* pattern=NULL;
         if(g_strcmp0(attr,"From:")==0)      pattern=g_strdup("From:\\s([a-zA-Z0-9\\-\\_\\.]+@[a-zA-Z0-9\\-\\_]+\\.+[a-zA-Z]+)\\R");
    else if(g_strcmp0(attr,"Date:")==0)      pattern=g_strdup("Date:\\s([a-zA-Z0-9,+ :]+)\\R");
    else if(g_strcmp0(attr,"Subject:")==0)   pattern=g_strdup("Subject:\\s(\\b[\\w\\s]+\\b)\\R");
    else if(g_strcmp0(attr,"Boundary:")==0)  pattern=g_strdup("Boundary:\\s(\\b\\w+\\b)\\R"); 
    else if(g_strcmp0(attr,"Charset:")==0)   pattern=g_strdup("Charset:([\\w\\d-_]+)\\R");
    else if(g_strcmp0(attr,"Content-Transfer-Encoding:")==0)   pattern=g_strdup("Content-Transfer-Encoding:\\s(.+?)\\R");
    else if(g_strcmp0(attr,"X-MRIM-Flags:")==0)  pattern=g_strdup("X-MRIM-Flags:\\s([a-fA-F0-9]+)\\R");
    else if(g_strcmp0(attr,"MSG")==0)
    {
        gchar* boundary = mrim_message_offline_get_attr("Boundary:",input);
        if(boundary==NULL || *boundary=='\0')
            pattern=g_strdup("\\R\\R(.+)");
        else
            pattern=g_strconcat("\\R\\R(.+?)\\R--",boundary,"--",NULL);
    }
    else
        return NULL;

    regex = g_regex_new (pattern, G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);

    res=g_regex_match (regex, (gchar*)input, 0, &match_info);
    if(res)
       retVal=g_match_info_fetch(match_info,1);
    
    purple_debug_info("mrim"," attr <%s> : <%s>\n",attr,retVal);

	// TODO Mem free.
	g_free(pattern);
	g_match_info_free(match_info);
	g_regex_unref(regex);
	return retVal;
}


time_t mrim_str_to_time(const gchar* str)
{	// TODO: Determine whether we need to transit from GMT-shift to UTC-shift
	// and implement such transition.
    int year=0,month=0,day=0,hour=0,min=0,sec=0;
    gchar month_str[4];
    int ret;
    if(str==NULL)
    {
        purple_debug_error("mrim","DATE sscanf error: str=NULL\n");
        return 0;
    }
    ret=sscanf(str,"%*03s, %u %03s %u %u:%u:%u",&day,month_str,&year,&hour,&min,&sec);
    if(ret!=6)
    {
        purple_debug_error("mrim","DATE sscanf error: str=%s\n",str);
        return 0;
    }
    if(g_strcmp0(month_str,"Jan")==0)      month=1;
    else if(g_strcmp0(month_str,"Feb")==0) month=2;
    else if(g_strcmp0(month_str,"Mar")==0) month=3;
    else if(g_strcmp0(month_str,"Apr")==0) month=4;
    else if(g_strcmp0(month_str,"May")==0) month=5;
    else if(g_strcmp0(month_str,"Jun")==0) month=6;
    else if(g_strcmp0(month_str,"Jul")==0) month=7;
    else if(g_strcmp0(month_str,"Aug")==0) month=8;
    else if(g_strcmp0(month_str,"Sep")==0) month=9;
    else if(g_strcmp0(month_str,"Oct")==0) month=10;
    else if(g_strcmp0(month_str,"Nov")==0) month=11;
    else if(g_strcmp0(month_str,"Dec")==0) month=12;
    else
    {
        purple_debug_error("mrim","DATE month error: str=%s\n",str);
        return 0;
    }
    purple_debug_info("mrim","DATE parsed: str=%s\n%u %u %u %u:%u:%u\n",str,day,month,year,hour,min,sec);
    return purple_time_build(year,month,day,hour,min,sec);
}
/******************************************
 *           *Normal messages.*
 ******************************************/
void mrim_read_im(mrim_data *mrim, package *pack)
{
	g_return_if_fail(mrim);
	g_return_if_fail(pack);
	g_return_if_fail(mrim->gc);
	PurpleConnection *gc = mrim->gc;

	guint32 msg_id = read_UL(pack);		// seq
	guint32 flag = read_UL(pack);		// flag - look at MRIM_CS_MESSAGE
	purple_debug_info("mrim","[%s] flags=<%x>\n", __func__, flag);
	if (flag & MESSAGE_FLAG_SPAMF_SPAM)
		purple_debug_info("mrim","[%s] Message is SPAM?\n",__func__);
	//return;
	gchar *from = read_LPS(pack);

/*	mrim_pq *pq = (mrim_pq *) g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq)); // or msg_id?
	if (pq == NULL)
	{
		purple_debug_info("mrim","Can't find pack in pq\n");
	}
	else
		g_hash_table_remove(mrim->pq,  GUINT_TO_POINTER(pack->header->seq));
*/
	if (!(flag & MESSAGE_FLAG_NORECV)) 
	{// Delivery confirmation.
		package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE_RECV);
		if (flag & MESSAGE_FLAG_SMS)
			add_LPS("mrim_sms@mail.ru",pack); //TODO What is that?
		else
			add_LPS(from,pack);
		add_ul(msg_id, pack);
		send_package(pack, mrim);
	}
	
	// CHATS
	//if (flag & MESSAGE_FLAG_MULTICHAT) // WTF?? WHY IT DOES'NT WORK???
	if (is_valid_chat(from))
	{
		purple_debug_info("mrim", "[%s] it is CHAT!\n",__func__);
		if (flag & MESSAGE_FLAG_AUTHORIZE)
		{


		}
		else
		{
			gchar *mes = read_LPS(pack);	// message
			// TODO unescape
			gchar** split = g_strsplit(mes,":\r\n",3);
			if (split && split[0])
			{
				purple_debug_info("mrim", "[%s] <%s>\n",__func__, split[0]);
				if (split[1])
				{
					purple_debug_info("mrim", "[%s] <%s>\n",__func__,split[1]);
					if (split[2])
						purple_debug_info("mrim", "[%s] <%s>\n",__func__,split[2]);

				}
			}
			// PurpleChat *pc = purple_blist_find_chat(gc->account, split[0]);
			PurpleConversation *pconv =	purple_conversation_new(PURPLE_CONV_TYPE_CHAT, gc->account, split[0]);
			int id = purple_conv_chat_get_id(purple_conversation_get_chat_data(pconv));
			serv_got_chat_in(gc, id, split[1], PURPLE_MESSAGE_RECV, split[2], time(NULL));
			g_strfreev(split);
		}

		return;
	}

	//
	if (flag & MESSAGE_FLAG_AUTHORIZE)
	{	// Auth request.
		purple_debug_info("mrim","[%s] auth\n", __func__);
		guint32 i;// mst be equal ==2; Two LSP
		gchar* authorize_alias = NULL;
		gchar* authorize_message = NULL;
		read_base64(pack, TRUE, "uss", &i, authorize_alias, authorize_message);

		auth_data *a_data = g_new0(auth_data, 1);
		a_data->from = g_strdup(from);
		a_data->seq = mrim->seq;
		a_data->mrim = mrim;
		gboolean is_in_blist = (purple_find_buddy(mrim->account,from) != NULL);
		purple_account_request_authorization(mrim->account, from, NULL, authorize_alias, authorize_message, is_in_blist, mrim_authorization_yes, mrim_authorization_no, a_data);
		FREE(authorize_message);
		FREE(authorize_alias);
		FREE(from);
		return;
	}

	gchar *mes = read_LPS(pack);	// message
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >5
	gchar *correct_message = purple_markup_escape_text (mes, -1);
#else
	gchar *correct_message = g_markup_escape_text(mes, -1);
#endif

	if ((flag & MESSAGE_FLAG_NOTIFY) || (strcmp(mes," ")==0)) // According to proto spec should check MESSAGE_FLAG_NOTIFY only.
	{	// A buddy types.
		purple_debug_info("mrim"," notify\n");
		serv_got_typing(mrim->gc, from, 10, PURPLE_TYPING);
		FREE(from);
		FREE(mes);
		return;
	}

	if ( flag & MESSAGE_FLAG_RTF )
	{
		if (flag & MESSAGE_FLAG_ALARM)
		{
			gchar *rtf = g_strdup(mes); // TODO read_base64
			serv_got_attention(gc, from, 0/*code*/);
			purple_debug_info("mrim", "Bzzz! <%ul>\n", flag);
			//serv_got_im(gc, from, rtf, PURPLE_MESSAGE_RECV , time(NULL));
			FREE(rtf);
		}
		else
		{
			purple_debug_info("mrim"," rtf\n");
			gchar *rtf = strdup(correct_message); // TODO read_base64
			serv_got_im(mrim->gc, from, rtf, PURPLE_MESSAGE_RECV , time(NULL));
			FREE(rtf);
		}
	}
	else
	{	// A buddy sent a message.
		purple_debug_info("mrim","[%s] simple message <%s>\n", __func__, correct_message);
		serv_got_im(mrim->gc, from, correct_message, PURPLE_MESSAGE_RECV, time(NULL));
	}
	FREE(mes);
	FREE(from);
	FREE(correct_message);
}

int mrim_send_im(PurpleConnection *gc, const char *to, const char *message, PurpleMessageFlags flags)
{
	mrim_data *mrim = gc->proto_data;
	if (gc->state != PURPLE_CONNECTED)
		return -ENOTCONN; // Not connected.
	
	purple_debug_info("mrim", "sending message from %s to %s: %s\n", mrim->username, to, message);

	
	gboolean res = FALSE;
	if (clear_phone((gchar*)to))
	{
		res = mrim_send_sms((gchar *)to, (gchar*)message, mrim);
	}
	else
	{
		// pq
		mrim_pq *mpq = g_new0(mrim_pq, 1);
		mpq->seq =  mrim->seq;
		mpq->type = MESSAGE;
		mpq->kap_count = mrim->kap_count;
		mpq->message.flags = flags;
		mpq->message.to = g_strdup(to);
		mpq->message.message = g_strdup(message);
		g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

		package *pack = new_package(mpq->seq, MRIM_CS_MESSAGE);
		add_ul(0, pack);
		add_LPS(mpq->message.to, pack);
		add_LPS(mpq->message.message, pack);
		add_LPS(" ", pack);
		//add_base64(pack, TRUE, "usuu", 2, rtf, 4, 0x00FFFFFF); // TODO NEXT RELEASE
		/*
		 * packStream << quint32(2);
		 * packStream << rtf;
		 * packStream << quint32(4);
		 * packStream << quint32(0x00FFFFFF);
		 */
		res = send_package(pack, mrim);
	}

	if (res) 
		return 1;	// > 0 if succeeded.
	else
		return -E2BIG; // Failed.	
}
/******************************************
 *       "Buddy types."
 ******************************************/
static const char *typing_state_to_string(PurpleTypingState typing)
{
  switch (typing) 
  {
  case PURPLE_NOT_TYPING:  return _("is not typing");
  case PURPLE_TYPING:      return _("is typing");
  case PURPLE_TYPED:       return _("stopped typing momentarily");
  default:                 return _("unknown typing state");
  }
}

unsigned int mrim_send_typing(PurpleConnection *gc, const char *name,PurpleTypingState typing) 
{
	purple_debug_info("mrim", "%s %s\n", gc->account->username, typing_state_to_string(typing));
	if (typing != PURPLE_TYPING)
		return 0; // No need to send anymore.

	mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_NOTIFY | MESSAGE_FLAG_NORECV, pack);
	add_LPS((gchar*)name, pack); // Destination. // TODO g_strdup??
	add_LPS(" ", pack);
	add_LPS(" ", pack);// With no RTF part.
	send_package(pack, mrim);
	return 9;// Need to send every 10 seconds.
}

/******************************************
 *           *Buzzer.*
 ******************************************/
gboolean mrim_send_attention(PurpleConnection  *gc, const char *username, guint type)
{
	purple_debug_info("mrim", "[%s] %s\n", __func__, gc->account->username);

	mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_ALARM | MESSAGE_FLAG_RTF , pack); 
	add_LPS(username, pack); // Destination.
	add_LPS("s", pack);
	add_raw(&buzz_mas, ARRAY_SIZE(buzz_mas), pack);
	send_package(pack, mrim);
	return TRUE;
}
/******************************************
 *
 ******************************************/
void mrim_message_status(package *pack)
{
	// TODO PQ
	gchar *mes;
	guint32 status = read_UL(pack);
	switch (status)
	{
		case MESSAGE_DELIVERED:
			mes = _("Message successfully delivered.");
			break;
		case MESSAGE_REJECTED_INTERR:
			mes = _("Internal error encountered.");
			break;
		case MESSAGE_REJECTED_NOUSER:
			mes = _("Recipient does not exist.");
			break;
		case MESSAGE_REJECTED_LIMIT_EXCEEDED:
			mes = _("Recipient is offline. Message can not be stored in his mailbox.");
			break;
		case MESSAGE_REJECTED_TOO_LARGE:
			mes = _("Message size exceeds maximal length allowed.");
			break;
		case MESSAGE_REJECTED_DENY_OFFMSG:
			mes = _("Recipient does not support offline messages.");
			break;
		case MESSAGE_REJECTED_DENY_OFFFLSH:
			mes = _("User does not accept offline flash animation");
			break;
		default:
			mes = _("Unknown status");
			break;
	}
	purple_debug_info("mrim","[%s] status_id=<%u> status=<%s>\n",__func__, status, mes);
}



/******************************************
 *                SMS
 ******************************************/
gboolean mrim_send_sms(gchar *phone, gchar *message, mrim_data *mrim)
{
	g_return_val_if_fail(mrim, FALSE);
	g_return_val_if_fail(phone, FALSE);
	g_return_val_if_fail(message, FALSE);
	gchar *correct_phone = clear_phone(phone);
	if (correct_phone)
		correct_phone = g_strdup_printf("+%s",correct_phone);
	else
		correct_phone = phone;

	size_t len = strlen(message);
	if (len<=1) // too short
	{
		purple_notify_info(_mrim_plugin, _("SMS"), _("Message is too short."), "");
		return FALSE;
	}
	else if (len >= 1024)// TODO Too long messages: en(135?) & others(37?)
	{
		purple_notify_info(_mrim_plugin, _("SMS"), _("Message is too long."), "");
		return FALSE;
	}
	purple_debug_info("mrim", "[%s] to=<%s> message=<%s>\n", __func__, phone, message);
	mrim_pq *mpq = g_new0(mrim_pq, 1);
	mpq->type = SMS;
	mpq->seq = mrim->seq;
	mpq->sms.phone = correct_phone;
	mpq->sms.message = g_strdup(message);
	g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);

	package *pack = new_package(mrim->seq, MRIM_CS_SMS);
	add_ul(0, pack);
	add_LPS(correct_phone, pack);
	add_LPS(message, pack);
	gboolean ret = send_package(pack, mrim);


	PurpleConversation *pc = purple_conversation_new(PURPLE_CONV_TYPE_UNKNOWN, mrim->account, phone);
	PurpleLog *pl = purple_log_new(PURPLE_LOG_IM /*PURPLE_LOG_SYSTEM*/, correct_phone, mrim->account, pc, time(NULL), NULL);
	// PURPLE_MESSAGE_INVISIBLE PURPLE_MESSAGE_SYSTEM PURPLE_MESSAGE_ACTIVE_ONLY
	purple_log_write(pl, PURPLE_MESSAGE_ACTIVE_ONLY, phone, time(NULL), message);
	purple_log_delete(pl);
	purple_conversation_destroy(pc);

	return ret;
}

void mrim_sms_ack(mrim_data *mrim ,package *pack)
{
	purple_debug_info("mrim","[%s]\n",__func__);

	guint32 status = read_UL(pack);
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));

	switch (status)
	{
		case MRIM_SMS_SERVICE_UNAVAILABLE: purple_notify_error(mrim->gc, _("SMS"), _("SMS service is not available"), _("SMS service is not available")); break;
		case MRIM_SMS_OK:purple_notify_info(mrim->gc, _("SMS"), _("SMS message sent."), _("SMS message sent.")); break;
		case MRIM_SMS_INVALID_PARAMS:purple_notify_error(mrim->gc, _("SMS"), _("Wrong SMS settings."), _("Wrong SMS settings."));break;
		default:purple_notify_error(mrim->gc, _("SMS"), _("Achtung!"), _("Anyone here?? !")); break;
	}
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}



/******************************************
 *                Chats
 ******************************************/
GList *mrim_chat_info(PurpleConnection *gc)
{
	purple_debug_info("mrim", "%s\n", __func__);

	struct proto_chat_entry *pce = g_new0(struct proto_chat_entry, 1); // defined in prpl.h
	GList *chat_info = NULL;
	pce->label = "Search";
	pce->identifier = "search";
	pce->required = FALSE;
	chat_info = g_list_append(chat_info, pce);
	return chat_info; // TODO pidgin bureport: if returned NULL, crash at purple_blist_find_chat>>parts = prpl_info->chat_info( purple_account_get_connection(chat->account));
}

GHashTable *mrim_chat_info_defaults(PurpleConnection *gc, const char *chat_name)
{
	purple_debug_info("mrim", "%s\n", __func__);

	GHashTable *defaults;
	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	g_hash_table_insert(defaults, "search", g_strdup(chat_name));
	return defaults; //defauts;
}

void mrim_chat_join(PurpleConnection *gc, GHashTable *components)
{
	purple_debug_info("mrim", "%s\n", __func__);
	purple_serv_got_join_chat_failed(gc, components);

	const char *username = gc->account->username;
	const char *room = g_hash_table_lookup(components, "room");
	//guint chat_id = g_str_hash(room);
	purple_debug_info("mrim", "%s is joining chat room %s\n", username, room);

	//if (!purple_find_chat(gc, chat_id))
	//{
		//serv_got_joined_chat(gc, chat_id, room);
	serv_got_joined_chat(gc, 99, "1213");

		/* tell everyone that we joined, and add them if they're already there */
		//foreach_gc_in_chat(joined_chat, gc, chat_id, NULL);
	//}
	//else
	{
		purple_debug_info("mrim", "%s is already in chat room %s\n", username, room);
	}
}

void mrim_reject_chat(PurpleConnection *gc, GHashTable *components)
{
	const char *invited_by = g_hash_table_lookup(components, "invited_by");
	const char *room = g_hash_table_lookup(components, "room");
	const char *username = gc->account->username;
	//PurpleConnection *invited_by_gc = get_nullprpl_gc(invited_by);
	char *message = g_strdup_printf("%s %s %s.", username,	_("has rejected your invitation to join the chat room"), room);

	purple_debug_info("mrim", "%s has rejected %s's invitation to join chat room %s\n", username, invited_by, room);

	g_free(message);
}

char *mrim_get_chat_name(GHashTable *components)
{
	purple_debug_info("mrim", "%s\n", __func__);
	char *str = g_strdup("mrim_chat");
	/*
	 const char *chat_type_str = g_hash_table_lookup(components, "chat_type");
	 TwitterChatType chat_type = chat_type_str == NULL ? 0 : strtol(chat_type_str, NULL, 10);
	 TwitterEndpointChatSettings *settings = twitter_get_endpoint_chat_settings(chat_type);
	 if (settings && settings->get_name)
	 return settings->get_name(components);
	 */
	return str;
	/*const char *room = g_hash_table_lookup(components, "room");
	 purple_debug_info("nullprpl", "reporting chat room name '%s'\n", room);
	 return g_strdup(room);
	 */
}

void mrim_chat_invite(PurpleConnection *gc, int id, const char *message, const char *who)
{
	purple_debug_info("mrim", "%s\n", __func__);

	const char *username = gc->account->username;
	PurpleConversation *conv = purple_find_chat(gc, id);
	const char *room = conv->name;
	PurpleAccount *to_acct = purple_accounts_find(who, MRIM_PRPL_ID);

	purple_debug_info("mrim", "%s is inviting %s to join chat room %s\n", username, who, room);

	if (to_acct)
	{
		PurpleConversation *to_conv = purple_find_chat(to_acct->gc, id);
		if (to_conv)
		{
			char *tmp = g_strdup_printf("%s is already in chat room %s.", who, room);
			purple_debug_info("mrim", "%s is already in chat room %s; ignoring invitation from %s\n", who, room, username);
			purple_notify_info(gc, _("Chat invitation"), _("Chat invitation"), tmp);
			g_free(tmp);
		}
		else
		{
			GHashTable *components;
			components = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
			g_hash_table_replace(components, "room", g_strdup(room));
			g_hash_table_replace(components, "invited_by", g_strdup(username));
			serv_got_chat_invite(to_acct->gc, room, username, message, components);
		}
	}
}

void mrim_chat_leave(PurpleConnection *gc, int id)
{
	PurpleConversation *conv = purple_find_chat(gc, id);
	purple_debug_info("mrim", "%s is leaving chat room %s\n", gc->account->username, conv->name);
	/* tell everyone that we left */
	//foreach_gc_in_chat(left_chat_room, gc, id, NULL);
}

void mrim_chat_whisper(PurpleConnection *gc, int id, const char *who, const char *message)
{
	purple_debug_info("mrim", "%s\n", __func__);
	const char *username = gc->account->username;
	PurpleConversation *conv = purple_find_chat(gc, id);
	purple_debug_info("mrim", "%s receives whisper from %s in chat room %s: %s\n", username, who, conv->name, message);

	/* receive whisper on recipient's account */
	serv_got_chat_in(gc, id, who, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_WHISPER, message, time(NULL));
}

int mrim_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	purple_debug_info("mrim", "%s\n", __func__);
	mrim_data *mrim = gc->proto_data;
	const char *username = gc->account->username;
	PurpleConversation *conv = purple_find_chat(gc, id);

	if (conv)
	{
		purple_debug_info("mrim", "%s is sending message to chat room %s: %s\n", username, conv->name, message);
		gboolean res;

		mrim_pq *mpq = g_new0(mrim_pq, 1);
		mpq->seq =  mrim->seq;
		mpq->type = MESSAGE;
		mpq->kap_count = mrim->kap_count;
		mpq->message.flags = MESSAGE_FLAG_NORECV | MESSAGE_FLAG_MULTICHAT;
		mpq->message.to = g_strdup(""); // TODO chatname
		mpq->message.message = g_strdup_printf("%s:\r\n%s", mrim->username, message);
		g_hash_table_insert(mrim->pq, GUINT_TO_POINTER(mpq->seq), mpq);


		package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
		add_ul(mpq->message.flags, pack); //flags
		add_LPS(mpq->message.to, pack);
		add_LPS(mpq->message.message, pack);
		add_LPS(" ", pack);

		res = send_package(pack, mrim);
		serv_got_chat_in(gc, id, username, flags, message, time(NULL));
		if (res)
			return 1;
		else
			return -E2BIG; // Failed.
	}
	else
	{
		purple_debug_info("mrim", "tried to send message from %s to chat room #%d: %s\n but couldn't find chat room", username, id, message);
		return -EINVAL; // todo why not -1?
	}
}

void mirm_set_chat_topic(PurpleConnection *gc, int id, const char *topic)
{
	purple_debug_info("mrim", "%s\n", __func__);
}

static void nullprpl_register_user(PurpleAccount *acct)
{
	purple_debug_info("mrim", "registering account for %s\n", acct->username);
}

static PurpleRoomlist *mrim_roomlist_get_list(PurpleConnection *gc)
{
	const char *username = gc->account->username;
	PurpleRoomlist *roomlist = purple_roomlist_new(gc->account);
	GList *fields = NULL;
	PurpleRoomlistField *field;
	GList *chats;
	GList *seen_ids = NULL;

	purple_debug_info("mrim", "%s asks for room list; returning:\n", username);

	/* set up the room list */
	field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "room",	"room", TRUE /* hidden */);
	fields = g_list_append(fields, field);

	field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, "Id", "Id", FALSE);
	fields = g_list_append(fields, field);

	purple_roomlist_set_fields(roomlist, fields);

	/* add each chat room. the chat ids are cached in seen_ids so that each room
	 * is only returned once, even if multiple users are in it. */
	for (chats = purple_get_chats(); chats; chats = g_list_next(chats))
	{
		PurpleConversation *conv = (PurpleConversation *) chats->data;
		PurpleRoomlistRoom *room;
		const char *name = conv->name;
		int id = purple_conversation_get_chat_data(conv)->id;

		/* have we already added this room? */
		if (g_list_find_custom(seen_ids, name, (GCompareFunc) strcmp))
			continue; /* yes! try the next one. */

		/* This cast is OK because this list is only staying around for the life
		 * of this function and none of the conversations are being deleted
		 * in that timespan. */
		seen_ids = g_list_prepend(seen_ids, (char *) name); /* no, it's new. */
		purple_debug_info("mrim", "%s (%d), ", name, id);

		room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
		purple_roomlist_room_add_field(roomlist, room, name);
		purple_roomlist_room_add_field(roomlist, room, &id);
		purple_roomlist_room_add(roomlist, room);
	}

	g_list_free(seen_ids);
	//purple_timeout_add(1 /* ms */, nullprpl_finish_get_roomlist, roomlist);
	return roomlist;
}

static void mrim_roomlist_cancel(PurpleRoomlist *list)
{
	purple_debug_info("mrim", "%s asked to cancel room list request\n",	list->account->username);
}

static void mrim_roomlist_expand_category(PurpleRoomlist *list,	PurpleRoomlistRoom *category)
{
	purple_debug_info("mrim", "%s asked to expand room list category %s\n", list->account->username, category->name);
}

/*
static PurpleRoomlist *irc_roomlist_get_list(PurpleConnection *gc)
{
	struct irc_conn *irc;
	GList *fields = NULL;
	PurpleRoomlistField *f;
	char *buf;

	irc = gc->proto_data;

	if (irc->roomlist)
		purple_roomlist_unref(irc->roomlist);

	irc->roomlist = purple_roomlist_new(purple_connection_get_account(gc));

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "channel", TRUE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, _("Users"), "users", FALSE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(irc->roomlist, fields);

	buf = irc_format(irc, "v", "LIST");
	irc_send(irc, buf);
	g_free(buf);

	return irc->roomlist;
}

static void irc_roomlist_cancel(PurpleRoomlist *list)
{
	PurpleConnection *gc = purple_account_get_connection(list->account);
	struct irc_conn *irc;

	if (gc == NULL)
		return;

	irc = gc->proto_data;

	purple_roomlist_set_in_progress(list, FALSE);

	if (irc->roomlist == list) {
		irc->roomlist = NULL;
		purple_roomlist_unref(list);
	}
}

*/
