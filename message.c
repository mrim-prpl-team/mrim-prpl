#include "mrim.h"
#include "message.h"
#include "package.h"
#include "cl.h"
/******************************************
 *           Оффлайн сообещния
 ******************************************/
// Чтение оффлайн сообщение
void mrim_message_offline(PurpleConnection *gc, char* message)
{
	purple_debug_info("mrim","parse offline message\n");
	if (!message)
		return;

    gchar* from = mrim_message_offline_get_attr("From:",message);
    gchar* date_str = mrim_message_offline_get_attr("Date:",message);
    gchar* charset = mrim_message_offline_get_attr("Charset:",message);
    gchar* msg = mrim_message_offline_get_attr("MSG",message);
	gchar* encoding = mrim_message_offline_get_attr("Content-Transfer-Encoding:",message);
    time_t date = mrim_str_to_time(date_str);
    gchar* correct_code=NULL;
    
    if (encoding)
    {		
		gchar* msg_decoded=NULL;
		gsize len_decoded=0;
		gsize len_correct=0;
		
		encoding = g_ascii_tolower( *g_strstrip(encoding) ); // TODO test
		if(encoding && g_strcmp0(encoding,"base64")==0)
		{
			msg_decoded = (gchar*) purple_base64_decode(msg, &len_decoded); // можно?
			len_correct = len_decoded;
			correct_code = g_memdup(msg_decoded,len_decoded+1);
			correct_code[len_decoded] = '\0';
			FREE(msg_decoded);
		}
	}

	if (correct_code)
		serv_got_im(gc, from, correct_code, PURPLE_MESSAGE_RECV, date);
	else if(msg)
		serv_got_im(gc, from, msg, PURPLE_MESSAGE_RECV, date);
	else if(message)
		serv_got_im(gc, from, message, PURPLE_MESSAGE_RECV, date);
    
	FREE(correct_code);
	FREE(from);
	FREE(date_str);
	FREE(charset);
	FREE(msg);
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

	// TODO осовободить память
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
 *           Обчычные сообщения
 ******************************************/
void mrim_read_im(mrim_data *mrim, package *pack)
{
	g_return_if_fail(mrim);
	g_return_if_fail(pack);
	g_return_if_fail(mrim->gc);
	PurpleConnection *gc = mrim->gc;

	guint32 msg_id = read_UL(pack);		// seq
	guint32 flag = read_UL(pack);		// flag - смотри MRIM_CS_MESSAGE
	//if (flag & MESSAGE_FLAG_SPAMF_SPAM)
	//	return;
	gchar *from = read_LPS(pack);
	
	mrim_pq *pq = (mrim_pq *) g_hash_table_lookup(mrim->pq, GUINT_TO_POINTER(pack->header->seq)); // или msg_id?
	if (pq == NULL)
	{
		purple_debug_info("mrim","Can't find pack in pq\n");
	}
	else
		g_hash_table_remove(mrim->pq,  GUINT_TO_POINTER(pack->header->seq));

	if (!(flag & MESSAGE_FLAG_NORECV)) 
	{// подтверждение доставки
		package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE_RECV);
		if (flag & MESSAGE_FLAG_SMS)
			add_LPS("mrim_sms@mail.ru",pack);
		else
			add_LPS(from,pack);
		add_ul(msg_id, pack);
		send_package(pack, mrim);
	}
	
	if (flag & MESSAGE_FLAG_AUTHORIZE)
	{	// запрос авторизации
		purple_debug_info("mrim"," auth\n");
		guint32 i;// mast be equal ==2; Two LSP
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
	if ((flag & MESSAGE_FLAG_NOTIFY) || (strcmp(mes," ")==0)) // по описанию протокола должен проверить только на MESSAGE_FLAG_NOTIFY
	{	// собеседник набирает сообщение
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
			gchar *rtf = strdup(mes); // TODO read_base64
			serv_got_im(mrim->gc, from, rtf, PURPLE_MESSAGE_RECV , time(NULL));
			FREE(rtf);
		}
	}

	else
	{	// собеседник прислал сообщение
		serv_got_im(mrim->gc, from, mes, PURPLE_MESSAGE_RECV , time(NULL));
	}
	FREE(mes);
	FREE(from);
}

int mrim_send_im(PurpleConnection *gc, const char *to, const char *message, PurpleMessageFlags flags)
{
	mrim_data *mrim = gc->proto_data;
	if (gc->state != PURPLE_CONNECTED)
		return -ENOTCONN; // не подключены
	
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

		package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
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
		return 1;	// > 0 если удачно
	else
		return -E2BIG; // неудачно	
}
/******************************************
 *       "Собеседник набирает сообщение"
 ******************************************/
static const char *typing_state_to_string(PurpleTypingState typing)
{
  switch (typing) 
  {
  case PURPLE_NOT_TYPING:  return "is not typing";
  case PURPLE_TYPING:      return "is typing";
  case PURPLE_TYPED:       return "stopped typing momentarily";
  default:               return "unknown typing state";
  }
}

unsigned int mrim_send_typing(PurpleConnection *gc, const char *name,PurpleTypingState typing) 
{
	purple_debug_info("mrim", "%s %s\n", gc->account->username, typing_state_to_string(typing));
	if (typing != PURPLE_TYPING)
		return 0; // больше не надо отсылать

	mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_NOTIFY | MESSAGE_FLAG_NORECV, pack);
	add_LPS((gchar*)name, pack); // кому // TODO g_strdup??
	add_LPS(" ", pack);
	add_LPS(" ", pack);// без RTF части
	send_package(pack, mrim);
	return 9;// надо отсылать каждые 10 секунд
}

/******************************************
 *           Будильник
 ******************************************/
gboolean mrim_send_attention(PurpleConnection  *gc, const char *username, guint type)
{
	purple_debug_info("mrim", "[%s] %s\n", __func__, gc->account->username);

	gchar *Username = g_strdup(username);
	mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_ALARM | MESSAGE_FLAG_RTF , pack); 
	add_LPS(Username, pack); // кому
	add_LPS(" Будильник ", pack);
	add_RTF(" ", pack); // RTF часть  // TODO
	send_package(pack, mrim);
	FREE(Username)
	return 10;// надо отсылать каждые 10 секунд	
	/*
	 *
	 mrim_data *mrim = gc->proto_data;
	package *pack = new_package(mrim->seq, MRIM_CS_MESSAGE);
	add_ul(MESSAGE_FLAG_ALARM | MESSAGE_FLAG_RTF , pack);
	add_LPS((gchar *) username, pack); // кому
	add_LPS(" Будильник ", pack);
	add_base64(pack, FALSE, "uss", 2, (gchar *)username, "Динь-Динь!"); // TODO RTF часть
	send_package(pack, mrim);
	return 9;// надо отсылать каждые 10 секунд
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
			mes = "Сообщение успешно доставлено";
			break;
		case MESSAGE_REJECTED_INTERR:
			mes = "Произошла внутренняя ошибка";
			break;
		case MESSAGE_REJECTED_NOUSER:
			mes = "Получатель не существует";
			break;
		case MESSAGE_REJECTED_LIMIT_EXCEEDED:
			mes = "Собеседник не в сети. Сообщение не будет помещено в его почтовый ящик";
			break;
		case MESSAGE_REJECTED_TOO_LARGE:
			mes = "Размер сообщения превышает максимально допустимый.";
			break;
		case MESSAGE_REJECTED_DENY_OFFMSG:
			mes = "Пользователь не поддерживает оффлайн сообщения";
			break;
		case MESSAGE_REJECTED_DENY_OFFFLSH:
			mes = "User does not accept offline flash animation";
			break;
		default:
			mes = "Unknown status";
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
		case MRIM_SMS_SERVICE_UNAVAILABLE: purple_notify_error(mrim->gc, "SMS", "Сервис отправки СМС-ок недоступен", "Сервис отправки СМС-ок недоступен"); break;
		case MRIM_SMS_OK:purple_notify_info(mrim->gc, "SMS", "СМС-ка была отправлена", "СМС-ка была отправлена"); break;
		case MRIM_SMS_INVALID_PARAMS:purple_notify_error(mrim->gc, "SMS", "Неверные параметры отправки СМС.", "Неверные параметры отправки СМС.");break;
		default:purple_notify_error(mrim->gc, "SMS", "Ахтунг!", "Кто здесь?? !"); break;
	}
	g_hash_table_remove(mrim->pq, GUINT_TO_POINTER(pack->header->seq));
}
