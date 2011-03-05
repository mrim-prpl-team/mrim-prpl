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
	for (int i=0; i<g_strlen(in_text);i++)
	{
		out_text = (out_text + in_text[i]);
	}
	return out_text;
}
