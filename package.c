/**   package.c
 * 
 *  Copyright (C) 2010, Антонов Николай (Antonov Nikolay) aka Ostin <antoa@mail.ru> 
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
//#ifndef PACKAGE_H
 //#define PACKAGE_H
 #include "package.h"
//#endif
// cURL


/******************************************
 *			   Output
 ******************************************/
package *new_package(guint32 seq, guint32 type)
{
	package *pack = g_new0(package, 1);
	// создаём header
	pack->header = g_new0(mrim_packet_header_t,1);
	pack->header->magic = CS_MAGIC;
	pack->header->proto = PROTO_VERSION;
	pack->header->seq = seq; 
	pack->header->msg = type; 
	pack->header->dlen = 0;
	pack->header->fromport = 0;
	pack->header->from = 0;	
	// заполняем остальное
	pack->buf = pack->cur = NULL;
	pack->len = 0;
	
#ifdef DEBUG
	purple_debug_info("mrim", "Create Package seq=<%u> type=<%x>\n", pack->header->seq, pack->header->msg);
#endif
	return pack;
}

void add_ul(guint32 ul, package *pack)
{
	if (pack == NULL)
		return;
	char *buf = g_new(char, pack->len + sizeof(guint32));
	// копируем текущее содержимое буфера
	g_memmove(buf, pack->buf, pack->len); // g_memmove(dest,src,len)
	g_free(pack->buf);
	pack->buf = buf;
	pack->cur = pack->buf + pack->len;
	// дописываем в конец UL
	g_memmove(pack->cur, &ul, sizeof(guint32));
	
	pack->len += sizeof(guint32);
#ifdef DEBUG
	purple_debug_info("mrim", "add_UL <%u>\n", ul);
#endif
}

void add_LPS(gchar *string, package *pack)
{
	if (pack == NULL)
		return;
	if (string == NULL)
	{
		add_ul(0, pack); // пустая строка
		return;
	}
	// изменяем кодировку
	gchar *str = g_convert_with_fallback(purple_unescape_html(string), -1, "CP1251" , "UTF8", NULL, NULL, NULL, NULL);

	if (! str)
	{
		purple_notify_warning(_mrim_plugin, "g_convert","Ошибка кодировки: не могу сконвертировать UTF8 в CP1251", "");
		return;
	}
	guint32 len = strlen(str);
	
	// копируем текущее содержимое буфера
	char *buf = g_new(char, pack->len + sizeof(guint32) + len*sizeof(char));
	g_memmove(buf, pack->buf, pack->len); // g_memmove(dest,src,len)
	g_free(pack->buf);
	pack->buf = buf;
	pack->cur = pack->buf + pack->len;
	// дописываем в конец нового буфера UL
	g_memmove(pack->cur, &len, sizeof(guint32));
	pack->cur += sizeof(guint32);
	// копируем строку в конец нового буфера
	int i = 0;
	while (str[i] != '\0')
		*(pack->cur++) = str[i++];
	
	pack->len += sizeof(guint32) + len;
#ifdef DEBUG
	purple_debug_info("mrim", "add_LPS <%s>\n", str);
#endif
	g_free(str);
}

void add_raw(char *new_data, int len, package *pack)
{
	g_return_if_fail(pack != NULL);
	g_return_if_fail(new_data != NULL);

#ifdef DEBUG
	purple_debug_info("mrim", "add_raw. len <%i>\n", len);
#endif
	// копируем текущее содержимое буфера
	char *buf = g_new(char, pack->len + len);
	g_memmove(buf, pack->buf, pack->len); // g_memmove(dest,src,len)
	g_free(pack->buf);
	pack->buf = buf;
	// копируем raw-данные в конец нового пакета
	pack->cur = pack->buf + pack->len;
	int i = 0;
	for (i=0; i<len; )
		*(pack->cur++) = new_data[i++];

	pack->len += len;
}

void add_base64(package *pack, gboolean gziped, gchar *fmt, ...)
{
	va_list ap;
	gchar *p, *str;
	guint32 u=0, len=0, str_len=0;
	/* Первый проход - считаем память */
	va_start(ap, fmt);
	for (p=fmt; *p; p++)
	{
		switch (*p)
		{
			case 'u':
				u = va_arg(ap, guint32);
				len += sizeof(guint32);
				break;
			case 's':
			case 'c': // cp1251
			case 'l': // UTF-16LE
				str = va_arg(ap, gchar *);
				len += sizeof(guint32);
				if (str)
					len += strlen(str);
				break;
			default:
				purple_debug_info("mrim","[%s] unknown argument \n", __func__);
				va_end(ap);
				return;
		}
	}
	/* второй - склеиваем */
	va_start(ap, fmt);
	char *buf = g_new(char, len);
	for (p=fmt; *p; p++)
	{
		switch (*p)
		{
			case 'u':
				u = va_arg(ap, guint32);
				g_memmove(buf, &u, sizeof(guint32));
#ifdef DEBUG
				purple_debug_info("mrim", "[%s] UL=<%u>\n",__func__, u);
#endif
				buf += sizeof(guint32);
				break;
			case 's':
			case 'c': // cp1251
			case 'l': // UTF-16LE
				str = va_arg(ap, gchar *);
				str_len = strlen(str);
				// дописываем UL
				g_memmove(buf, &str_len, sizeof(guint32));
				buf += sizeof(guint32);
				// копируем строку в конец нового буфера
				if (str)
				{
					int i = 0;
					while (str[i]) // TODO memmove ?
						*(buf++) = str[i++];
				}
#ifdef DEBUG
				purple_debug_info("mrim", "[%s] LPS=<%s>\n",__func__, str);
#endif
				break;
			default:
				purple_debug_info("mrim","[%s] error\n", __func__);
				va_end(ap);
				return;
		}
	}
	buf -= len; // вернули указатель в начало
	va_end(ap);
	// TODO gzip
	if (gziped)
	{

	}
	/* кодируем */
	gchar *encoded = purple_base64_encode((guchar*)buf, len); // TODO аккуратнее со знаковостью
	guint32 encoded_len = strlen(encoded);
	/* добавляем в пакет */
	add_ul(encoded_len, pack);
	add_raw(encoded, encoded_len, pack);
	FREE(encoded)
}

void add_RTF(gchar *string, package *pack)
{
	//TODO RTF
	add_LPS(string, pack);
}

void free_package(package *pack)
{
	if (pack != NULL)
	{
		if (pack->header != NULL)
			g_free(pack->header);
		if (pack->buf != NULL)
			g_free(pack->buf);
		g_free(pack);
	}
}

gboolean send_package(package *pack, mrim_data *mrim)
{
	g_return_val_if_fail(pack != NULL, FALSE);
	g_return_val_if_fail(mrim != NULL, FALSE);

	// подправляем длину
	pack->header->dlen = pack->len;	
	
	ssize_t ret1 = write (mrim->fd, pack->header, sizeof(mrim_packet_header_t));
	ssize_t ret2 = write (mrim->fd, pack->buf, pack->len);
	if ( (ret1 < sizeof(mrim_packet_header_t)) || (ret2 < (pack->len)) )
	{
		purple_debug_info("mrim", "[%s] error\n", __func__);
		free_package(pack);
		// TODO logout
		purple_timeout_remove(mrim->keep_alive_handle);// Больше не посылаем KA .тип gboolean.
		mrim->keep_alive_handle = 0;
		
		PurpleConnection *gc = mrim->gc;
		purple_input_remove(gc->inpa); // больше не принимаем пакеты
		gc->inpa = 0;
		purple_connection_error_reason (gc,	PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "[send_package] error");
		purple_connection_set_state(gc, PURPLE_DISCONNECTED);
		return FALSE;
	}
	purple_debug_info("mrim", "Отправил пакет len=<%i>\n", pack->len + sizeof(mrim_packet_header_t));
	free_package(pack);
	(mrim->seq)++;
	return TRUE;
}

/******************************************
 *             Input
 ******************************************/
static mrim_packet_header_t *read_header(mrim_data *mrim)
{
	mrim_packet_header_t *header = g_new0(mrim_packet_header_t, 1);
	// читаем заголовок
	ssize_t ret=0;

	ret = recv(mrim->fd, header, sizeof(mrim_packet_header_t), RECV_FLAGS);

	
	if (ret < sizeof(mrim_packet_header_t))
		{
			g_free(header);
			return NULL;
		}
	if (header->magic == CS_MAGIC)		
		return header;
	else
	{
		g_free(header);
		return NULL;
	}
}

package *read_package(mrim_data *mrim)
{
	ssize_t ret = 0;
	
	if (mrim->inp_package != NULL)
	{
		// дочитываем пакет
		package *pack = mrim->inp_package;
		ssize_t size = pack->len - (pack->cur - pack->buf);
		ret = recv(mrim->fd, pack->cur, size, RECV_FLAGS);
#ifdef DEBUG
		purple_debug_info( "mrim", "[%s] ret=<%ui> \n", __func__, ret);
#endif

		if (ret > 0)
		{		
			if (ret < size)
			{
				pack->cur += ret;
				return NULL;
			}
			else 
			{
				pack->cur = pack->buf;
				mrim->inp_package = NULL;
				return pack;
			}
		}
	}
	else
	{
		package *pack = g_new0(package, 1);
		// читаем новый пакет
		pack->header = read_header(mrim);
		if (pack->header == NULL)
		{
			g_free(pack);
			purple_debug_info("mrim","неправильный header\n");
			return NULL;
		}
		purple_debug_info( "mrim", "Читаю пакет seq=<%u> Тип=<%x> длина=<%u> \n",pack->header->seq, pack->header->msg, pack->header->dlen);
		pack->len = pack->header->dlen;
		pack->buf = g_new0(char, pack->len);
		pack->cur = pack->buf; 
		// читаем буфер
		ret = recv(mrim->fd, pack->buf, pack->len, RECV_FLAGS);
#ifdef DEBUG
		purple_debug_info( "mrim", "[%s] ret=<%u> \n", __func__, ret);
#endif
		
		if ((ret < (pack->len)) && (ret>0))
		{
			pack->cur += ret;
			mrim->inp_package = pack;
			return NULL;
		}
		if (ret == pack->len) 
			return pack;
	}
	
	if (ret < 0)
	{
		purple_connection_error(mrim->gc, "Read Error!");
		return NULL;
	}
	if (ret == 0)
	{
		purple_connection_error(mrim->gc, "Peer closed connection");
		return NULL;
	}
	return NULL;
}

guint32 read_UL(package *pack)
{
	if (pack == NULL)
		return 0;
	
	if ((pack->cur + sizeof(guint32))	<=	(pack->buf + pack->len)) // если не выходим за границы
	{
		guint32 ul =  *( (guint32*)(pack->cur) );
		pack->cur += sizeof(guint32);
#ifdef DEBUG
		purple_debug_info( "mrim", "[%s] <%u> \n", __func__, ul);
#endif
		return ul;
	}
	else
	{
		purple_debug_info( "mrim", "read_UL: В буфере недостаточно данных!\n");
		return 0;
	}
}

gchar *read_rawLPS(package *pack)
{
	if (pack == NULL)
		return NULL;
	guint32 len = read_UL(pack);
	if ((len == 0) || (len >= PACK_MAX_LEN))
		return NULL;
	
	if ((pack->cur+len) <= (pack->buf+pack->len))
	{
		gchar *str = g_new(gchar,len+1);
		g_memmove(str, pack->cur, len);
		pack->cur += len;
		str[len]='\0';
		return str;
	}	
	else
	{
		pack->cur = pack->buf + pack->len;
		purple_debug_info( "mrim", "read_rawLPS: В буфере недостаточно данных!\n");
		return NULL;
	}
}

gchar *read_LPS(package *pack)
{
	if (pack == NULL)
		return NULL;
	gchar *raw_str = read_rawLPS(pack);
	if (raw_str == NULL)
		return NULL;
	// ИЗМЕНЯЕМ КОДИРОВКУ
	gchar *string = g_convert(raw_str, -1, "UTF8", "CP1251", NULL, NULL, NULL);
	g_free(raw_str);
#ifdef DEBUG
	purple_debug_info( "mrim", "Считал строку! <%s> \n", string);
#endif
		return string;
}

gchar *read_UTF16LE(package *pack)
{
	if (pack == NULL)
		return NULL;
	gchar *str = read_rawLPS(pack);
	if (str == NULL)
		return NULL;
		
	return str;

	// ИЗМЕНЯЕМ КОДИРОВКУ
	gchar *string = g_convert(str, -1, "UTF8", "UTF-16LE", NULL, NULL, NULL);
	g_free(str);
#ifdef DEBUG
	purple_debug_info( "mrim", "Считал строку! <%s>\n",string);
#endif
	return string;
}

gchar *read_Z(package *pack)
{
	if (pack == NULL)
		return NULL;
	// ПЕРЕПИСАТЬ!! ЭТО ВСЕГО ЛИШЬ ЗАГЛУШКА!
	gchar *str = "заглушка";
	purple_debug_info( "mrim", "read_Z: надо переписать функцию!\n");
	return str;
}


void read_base64(package *pack, gboolean gziped, gchar *fmt, ...)
{
	// TODO в случае неудачи обнулять все параметры
	va_list ap;
	va_start(ap, fmt);
	if (pack == NULL)
	{
		va_end(ap);
		return;
	}

	guint32 len = read_UL(pack);
	if ((len == 0) || (len >= PACK_MAX_LEN ))
	{
		va_end(ap);
		pack->cur += len;
		return;
	}


	gchar *p, *str;
	guchar *decoded, *buf;
	gsize decoded_len = len;
	guint32 *u;
	guint32 str_len;

	if ((pack->cur) <= (pack->buf+pack->len))
	{
		// декодируем
		decoded = purple_base64_decode(pack->cur,  &decoded_len); // TODO аккуратнее со знаковостью
		#ifdef DEBUG
		purple_debug_info("mrim", "[%s] decoded_len=<%u>\n",__func__, decoded_len);
		int i=0;
		for (i=0; i< decoded_len; i+=4)
			purple_debug_info("mrim", "[%s] %x\n",__func__, decoded[i]);
		#endif
	/*	if (gziped)
		{
			int ret, flush;
			unsigned have;
			z_streamp strm;
			unsigned char in[CHUNK];
			unsigned char out[CHUNK];

			strm->zalloc = Z_NULL;
			strm->zfree = Z_NULL;
			strm->opaque = Z_NULL;
			strm->avail_in = 0;
			strm->next_in = Z_NULL;
			ret = inflateInit_(strm, zlibVersion(), CHUNK);
			if (ret != Z_OK)
			{
				#ifdef DEBUG
				purple_debug_info("mrim", "[%s] Can not init zlib\n",__func__);
				#endif
				va_end(ap);
				return;
			}
			do
			{
				ret = !Z_STREAM_END;

			}while (ret != Z_STREAM_END);
		}*/
	}
	else
	{
		purple_debug_info( "mrim", "read_base64: В буфере недостаточно данных!\n");
		va_end(ap);
		return;
	}


	buf = decoded;
	for (p=fmt; *p; p++)
	{
		switch (*p)
		{
			case 'u':
				u = va_arg(ap, guint32 *);
				g_memmove(u, buf, sizeof(guint32));
				buf += sizeof(guint32);
				#ifdef DEBUG
					purple_debug_info("mrim", "[%s] UL=<%u>\n",__func__, *u);
				#endif
				break;
			case 's':
				g_memmove(&str_len, buf, sizeof(guint32));
				buf += sizeof(guint32);

				str = va_arg(ap, gchar *);
				if ((str_len == 0) || (buf+str_len) > (decoded+decoded_len))
				{
					str = NULL;
					purple_debug_info( "mrim", "[%s] error. len=<%u>\n",__func__, str_len);
					break;
				}

				gchar *str = g_new(gchar,str_len+1);
				g_memmove(str, buf, str_len);
				str[str_len]='\0';
				buf += str_len;
				#ifdef DEBUG
					purple_debug_info("mrim", "[%s] LPS=<%s>\n",__func__, str);
				#endif
				break;
			default:
				purple_debug_info("mrim","[%s] error: unknown type <%c>\n", __func__,*p);
				va_end(ap);
				return;
		}
	}
	va_end(ap);
	return;
}




