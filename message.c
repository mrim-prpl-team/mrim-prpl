#include "mrim.h"
#include "message.h"
#include "package.h"
#include "cl.h"

guint32 atox(gchar *str)
{
	g_return_val_if_fail(str,0);
	purple_debug_info("mrim","[%s] <%s>\n",__func__,str);
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
	purple_debug_info("mrim","[%s] <%x>\n",__func__,res);
	return res;
}

/******************************************
 *           *Offline messages.*
 ******************************************/
// Offline message reading...
void mrim_message_offline(PurpleConnection *gc, char* message)
{
	mrim_data *mrim = gc->proto_data;
	purple_debug_info("mrim","parse offline message\n");
	if (!message)
		return;

	gchar* from = mrim_message_offline_get_attr("From:",message);
	gchar* date_str = mrim_message_offline_get_attr("Date:",message);
	gchar* charset = mrim_message_offline_get_attr("Charset:",message);
	gchar* msg = mrim_message_offline_get_attr("MSG",message);
	gchar* encoding = mrim_message_offline_get_attr("Content-Transfer-Encoding:",message);
	time_t date = mrim_str_to_time(date_str);
	gchar* flags = mrim_message_offline_get_attr("X-MRIM-Flags:",message);
	guint32 mrim_flags = atox(flags);
	gchar* correct_code=NULL;
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

static gchar* mrim_message_offline_get_attr(const gchar* attr,void* input)
{
    char* retVal=NULL;
    GRegex *regex;
    gboolean res;
    GMatchInfo *match_info;

    gchar* pattern=NULL;
         if(g_strcmp0(attr,"From:")==0)      pattern=g_strdup("From:\\s([a-zA-Z0-9\\-\\_\\.]+@[a-zA-Z0-9\\-\\_]+\\.+[a-zA-Z]+)\\R");
    else if(g_strcmp0(attr,"Date:")==0)      pattern=g_strdup("Date:\\s([a-zA-Z0-9, :]+)\\R");
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
    g_match_info_free (match_info);
    g_regex_unref (regex);
    return retVal;
}


static time_t mrim_str_to_time(const gchar* str)
{
    guint year=0,month=0,day=0,hour=0,min=0,sec=0;
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
	//if (flag & MESSAGE_FLAG_SPAMF_SPAM)
	//	return;
	gchar *from = read_LPS(pack);
	
	mrim_pq *pq = (mrim_pq *) g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq)); // or msg_id?
	if (pq == NULL)
	{
		purple_debug_info("mrim","Can't find pack in pq\n");
	}
	else
		g_hash_table_remove(mrim->pq,  GUINT_TO_POINTER(pack->header->seq));

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
	
	if (flag & MESSAGE_FLAG_AUTHORIZE)
	{	// Auth request.
		purple_debug_info("mrim"," auth\n");
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

	gchar *Username = g_strdup(username);
	mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_ALARM | MESSAGE_FLAG_RTF , pack); 
	add_LPS(Username, pack); // Destination.
	add_LPS(" Будильник ", pack);
	add_RTF(" ", pack); // RTF part  // TODO
	send_package(pack, mrim);
	FREE(Username)
	return 10;// Need to send every 10 seconds.
	/*
	 *
	 mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_ALARM | MESSAGE_FLAG_RTF , pack);
	add_LPS((gchar *) username, pack); // Destination.
	add_LPS(" Будильник ", pack);
	add_base64(pack, FALSE, "uss", 2, (gchar *)username, "Динь-Динь!"); // TODO RTF part.
	send_package(pack, mrim);
	return 9;// Need to send every 10 seconds.
	 */
}
/******************************************
 *
 ******************************************/
void mrim_message_status(mrim_data *mrim, package *pack)
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
	//else send:
	// TODO Too long messages.
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
	return ret;
}

void mrim_sms_ack(mrim_data *mrim ,package *pack)
{
	purple_debug_info("mrim","[%s]\n",__func__);

	guint32 status = read_UL(pack);
	mrim_pq *mpq = g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq));

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
/*
static GList *mrim_chat_info(PurpleConnection *gc)
{
        struct proto_chat_entry *pce; // defined in prpl.h
        GList *chat_info = NULL;

        pce = g_new0(struct proto_chat_entry, 1);
        pce->label = "Search";
        pce->identifier = "search";
        pce->required = FALSE;

        chat_info = g_list_append(chat_info, pce);

        pce = g_new0(struct proto_chat_entry, 1);
        pce->label = "Update Interval";
        pce->identifier = "interval";
        pce->required = TRUE;
        pce->is_int = TRUE;
        pce->min = 1;
        pce->max = 60;

        chat_info = g_list_append(chat_info, pce);

        return chat_info;
}

GHashTable *twitter_chat_info_defaults(PurpleConnection *gc, const char *chat_name)
{
        GHashTable *defaults;

        defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

        g_hash_table_insert(defaults, "search", g_strdup(chat_name));

        //bug in pidgin prevents this from working
        g_hash_table_insert(defaults, "interval",
                        g_strdup_printf("%d", twitter_option_search_timeout(purple_connection_get_account(gc))));
        return defaults;
}
*/
