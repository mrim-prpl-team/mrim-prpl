/**   mrim-util.c of mrim-prpl project.
* Contact List management routines.
* Committed by Reslayer@mail.ru aka Reslayer.
*/
#include "mrim-util.h"

/******************************************
 *           *Coonverters*
 ******************************************/
/* Convert a string from non-latin spelling to latin one. */
gchar *mrim_transliterate(const gchar *src_text, const gchar *locale /* Optional */)
{
	gchar *in_text = g_strdup(src_text);
	gchar *out_text = g_strdup("");
	for (int i=0; i<strlen(in_text);i++)
	{
		out_text = (out_text + in_text[i]);
	}
	return out_text;
}


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
    ret=sscanf(str,"%*03s, %i %03s %i %i:%i:%i",&day,month_str,&year,&hour,&min,&sec);
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
 *           *String functions*
 ******************************************/
gchar *mrim_str_non_empty(gchar *str)
{
	if (str && *str)
		return g_strdup(str);
	else
		return NULL;
}

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


/******************************************
 *         *Email/chat/phone*
 ******************************************/
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

// mrim normailze
const char *mrim_normalize(const PurpleAccount *account, const char *who)
{
	g_return_val_if_fail(who != NULL, NULL);
	gchar *result = g_utf8_strdown(who, -1); //purple_normalize_nocase(account, who);
#ifdef DEBUG
	//purple_debug_info("mrim", "[%s] %s -> %s\n", __func__, who, result);
#endif
	return result;
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

/******************************************
 *                *Other*
 ******************************************/
void mrim_open_myworld_url(gchar *username, gchar *url)
{
	g_return_if_fail(username != NULL);
	g_return_if_fail(url != NULL);

	gchar *p=NULL;
    gchar** split = g_strsplit(username,"@",2);
	gchar *name = g_strdup(split[0]);
	gchar *domain =p= g_strdup(split[1]);
	if (domain)
	{
		while(*domain)
			domain++;
		while((*domain != '.')&&(domain > p)) // TODO corp.mail.ru
			domain--; // TODO segfault
		*domain = 0;
	}
	domain = p;
	g_strfreev(split);
	purple_debug_info("mrim","[%s] d<%s> n<%s>\n",__func__, domain, name);
	gchar *full_url = g_strdup_printf(url, domain, name);
	purple_notify_uri(_mrim_plugin, full_url);
}
