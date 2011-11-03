#include "mrim.h"
#include "package.h"
#include <stdarg.h>

MrimPackage *mrim_package_new(guint32 seq, guint32 type) {
	MrimPackage *pack = g_new0(MrimPackage, 1);
	pack->header = g_new0(mrim_packet_header_t, 1);
	pack->header->magic = CS_MAGIC;
	pack->header->proto = PROTO_VERSION;
	pack->header->seq = seq;
	pack->header->msg = type;
	return pack;
}

void mrim_package_free(MrimPackage *pack) {
	g_free(pack->header);
	g_free(pack->data);
	g_free(pack);
}

gboolean mrim_package_send(MrimPackage *pack, MrimData *mrim) {
	pack->header->dlen = pack->data_size;
	gsize ret, size;
	if (pack->data) {
		size = sizeof(mrim_packet_header_t) + pack->data_size;
		gchar *buffer = g_new(gchar, size);
		g_memmove(buffer, pack->header, sizeof(mrim_packet_header_t));
		g_memmove(buffer + sizeof(mrim_packet_header_t), pack->data, pack->data_size);
		ret = send(mrim->fd, buffer, size, 0);
		g_free(buffer);
	} else {
		size = sizeof(mrim_packet_header_t);
		ret = send(mrim->fd, pack->header, size, 0);
	}
	purple_debug_info("mrim-prpl", "[%s] Package sent: type is 0x%x, seq is %i\n", __func__,
		pack->header->msg, pack->header->seq);
	mrim_package_free(pack);
	return (ret >= size);
}

static mrim_packet_header_t *read_header(MrimData *mrim)
{
	mrim_packet_header_t *header = g_new0(mrim_packet_header_t, 1);
	gsize ret = recv(mrim->fd, header, sizeof(mrim_packet_header_t), 0);
	if (ret < sizeof(mrim_packet_header_t)) {
		g_free(header);
		purple_debug_info("mrim-prpl", "[%s] Package header len is %d instead of %d\n", __func__, ret, sizeof(mrim_packet_header_t));
		return NULL;
	}
	if (header->magic == CS_MAGIC) {
		return header;
	} else {
		purple_debug_info("mrim-prpl", "[%s] Package header MAGIC is 0x%x instead of 0x%x\n", __func__, header->magic, CS_MAGIC);
		g_free(header);
		return NULL;
	}
}

MrimPackage *mrim_package_read(MrimData *mrim) {
	ssize_t ret = 0;
	if (mrim->inp_package) {
		MrimPackage *pack = mrim->inp_package;
		gsize size = pack->data_size - pack->cur;
		ret = recv(mrim->fd, pack->data + pack->cur, size, 0);
		if (ret > 0) {		
			if (ret < size) {
				pack->cur += ret;
				return NULL;
			} else {
				pack->cur = 0;
				mrim->inp_package = NULL;
				return pack;
			}
		}
	} else {
		MrimPackage *pack = g_new0(MrimPackage, 1);
		pack->header = read_header(mrim);
		if (pack->header == NULL) {
			g_free(pack);
			return NULL;
		}
		purple_debug_info("mrim-prpl", "[%s] seq = %u, type = 0x%x len = %u\n", __func__, pack->header->seq, pack->header->msg, pack->header->dlen);
		pack->data_size = pack->header->dlen;
		pack->data = g_new0(char, pack->data_size);
		pack->cur = 0;
		ret = recv(mrim->fd, pack->data, pack->data_size, 0);
		if ((ret < (pack->data_size)) && (ret > 0)) {
			pack->cur += ret;
			mrim->inp_package = pack;
			return NULL;
		}
		if (ret == pack->data_size) {
			return pack;
		}
	}
	if (ret < 0) {
		purple_connection_error(mrim->gc, _("Read Error!") );
		return NULL;
	}
	if (ret == 0) {
		purple_connection_error(mrim->gc, _("Peer closed connection"));
		return NULL;
	}
	return NULL;
}

/* Package parsing */

gboolean mrim_package_read_raw(MrimPackage *pack, gpointer buffer, gsize size) {
	if (pack && pack->data) {
		if (pack->cur + size <= pack->data_size) {
			g_memmove(buffer, pack->data + pack->cur, size);
			pack->cur += size;
			return TRUE;
		} else {
			purple_debug_info("mrim-prpl", "[%s] Insufficient data in the buffer\n", __func__);
			return FALSE;
		}
	} else {
		purple_debug_info("mrim-prpl", "[%s] Null buffer!\n", __func__);
		return FALSE;
	}
}

guint32 mrim_package_read_UL(MrimPackage *pack) {
	guint32 result;
	if (mrim_package_read_raw(pack, &result, sizeof(result))) {
		return result;
	} else {
		return 0;
	}
}

gchar *mrim_package_read_LPSA(MrimPackage *pack) {
	gsize str_len = mrim_package_read_UL(pack);
	if (str_len) {
		gchar *str = g_new(gchar, str_len + 1);
		mrim_package_read_raw(pack, str, str_len);
		str[str_len] = '\0';
		gchar *string = g_convert(str, -1, "UTF8" , "CP1251", NULL, NULL, NULL);
		g_free(str);
		return string;
	} else {
		return NULL;
	}
}

gchar *mrim_package_read_LPSW(MrimPackage *pack) {
	gsize str_len = mrim_package_read_UL(pack);
	if (str_len) {
		gunichar2 *str = g_new(gunichar2, str_len / sizeof(gunichar2) + 1);
		mrim_package_read_raw(pack, str, str_len);
		str[str_len / sizeof(gunichar2)] = '\0';
		gchar *string = g_utf16_to_utf8(str, -1, NULL, NULL, NULL);
		g_free(str);
		return string;
	} else {
		return NULL;
	}
}

gchar *mrim_package_read_UIDL(MrimPackage *pack) {
	gchar *uidl = g_new(gchar, 8);
	mrim_package_read_raw(pack, uidl, 8);
	return uidl;
}

gchar *mrim_package_read_LPS(MrimPackage *pack) {
	gsize str_len = mrim_package_read_UL(pack);
	if (str_len) {
		gpointer str = g_new(gchar, str_len);
		mrim_package_read_raw(pack, str, str_len);
		gboolean valid_utf16 = TRUE;
		{
			gunichar2 *string = str;
			glong i;
			for (i = 0; i < (str_len / 2); i++) {
				gunichar ch = string[i];
				purple_debug_info("mrim-prpl", "[%s] Is char 0x%x defined??\n", __func__, ch);
				if ((!g_unichar_isdefined(ch)) || (ch >= 0xE000) && (ch < 0xF900)) {
					valid_utf16 = FALSE;
					break;
				}
			}
		}
		gchar *string;
		if (valid_utf16) {
			string = g_utf16_to_utf8(str, str_len / 2, NULL, NULL, NULL);
		} else {
			string = g_convert(str, str_len, "UTF8" , "CP1251", NULL, NULL, NULL);
		}
		g_free(str);
		return string;
	} else {
		return NULL;
	}
}

/* Package creating */

void mrim_package_add_raw(MrimPackage *pack, gchar *data, gsize data_size) {
	if (pack) {
		if (pack->data) {
			pack->data = g_realloc(pack->data, pack->data_size + data_size);
			g_memmove(pack->data + pack->data_size, data, data_size);
			pack->data_size += data_size;
		} else {
			pack->data = g_memdup(data, data_size);
			pack->data_size = data_size;
		}
	}
}

void mrim_package_add_UL(MrimPackage *pack, guint32 value) {
	mrim_package_add_raw(pack, (gchar*)&value, sizeof(value));
}

void mrim_package_add_LPSA(MrimPackage *pack, gchar *string) {
	gsize str_len;
	gchar *str = g_convert_with_fallback(string, -1, "CP1251" , "UTF8", NULL, NULL, &str_len, NULL);
	if (str) {
		mrim_package_add_UL(pack, str_len);
		mrim_package_add_raw(pack, str, str_len);
		g_free(str);
	} else {
		mrim_package_add_UL(pack, 0);
	}
}

void mrim_package_add_LPSW(MrimPackage *pack, gchar *string) {
	glong str_len;
	gunichar2 *str = g_utf8_to_utf16(string, -1, NULL, &str_len, NULL);
	if (str) {
		mrim_package_add_UL(pack, str_len * sizeof(gunichar2));
		mrim_package_add_raw(pack, (gchar*)str, str_len * sizeof(gunichar2));
		g_free(str);
	} else {
		mrim_package_add_UL(pack, 0);
	}
}

void mrim_package_add_UIDL(MrimPackage *pack, gchar *uidl) {
	mrim_package_add_raw(pack, uidl, 8);
}

void mrim_package_add_base64(MrimPackage *pack, gchar *fmt, ...) {
	gchar *buffer = NULL;
	gsize buffer_size = 0;
	va_list ap;
	va_start(ap, fmt);
	while (*fmt) {
		switch (*fmt) {
			case 'u':
				{
					guint32 value = va_arg(ap, guint32);
					buffer = g_realloc(buffer, buffer_size + sizeof(guint32));
					g_memmove(buffer + buffer_size, &value, sizeof(guint32));
					buffer_size += sizeof(guint32);
				}			
				break;
			case 's': //CP1251
				{
					gchar *string = va_arg(ap, gchar*);
					gsize str_len = g_utf8_strlen(string, -1);
					buffer = g_realloc(buffer, buffer_size + sizeof(guint32) + str_len);
					g_memmove(buffer + buffer_size, &str_len, sizeof(guint32));
					gchar *str = g_convert_with_fallback(string, -1, "CP1251" , "UTF8", NULL, NULL, NULL, NULL);
					g_memmove(buffer + buffer_size + sizeof(guint32), str, str_len);
					g_free(str);
					buffer_size += sizeof(guint32) + str_len;
				}
				break;
			case 'w': //UTF16
				{
					gchar *string = va_arg(ap, gchar*);
					gsize str_len = g_utf8_strlen(string, -1) * sizeof(gunichar2);
					buffer = g_realloc(buffer, buffer_size + sizeof(guint32) + str_len);
					g_memmove(buffer + buffer_size, &str_len, sizeof(guint32));
					gunichar2 *str = g_utf8_to_utf16(string, -1, NULL, NULL, NULL);
					g_memmove(buffer + buffer_size + sizeof(guint32), str, str_len);
					g_free(str);
					buffer_size += sizeof(guint32) + str_len;
				}
				break;
		}
		fmt++;
	}
	va_end(ap);
	gchar *encoded = purple_base64_encode((gchar*)buffer, buffer_size);
	guint32 encoded_len = strlen(encoded);
	mrim_package_add_UL(pack, encoded_len);
	mrim_package_add_raw(pack, encoded, encoded_len);
	g_free(encoded);
	g_free(buffer);
}
