/**   convert.c of mrim-prpl project.
* Contact List management routines.
* Committed by Reslayer@mail.ru aka Reslayer.
*/
#include "convert.h"


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
