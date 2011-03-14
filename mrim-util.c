/**   mrim-util.c of mrim-prpl project.
* Contact List management routines.
* Committed by Reslayer@mail.ru aka Reslayer.
*/
#include "mrim-util.h"


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




gchar *mrim_str_non_empty(gchar *str)
{
	if (str)
	{
		if (strnlen(str,1) > 0)
		{
			return g_strdup(str);
		} else
			return NULL;
	} else
		return NULL;
}
