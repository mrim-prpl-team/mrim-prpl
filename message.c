#include "mrim.h"
#include "package.h"
#include "message.h"
#include "util.h"
#include "cl.h"
#include <errno.h>

void mrim_message_ack(MrimData *mrim, gpointer data, MrimPackage *pack) {
	g_return_if_fail(pack->header->msg == MRIM_CS_MESSAGE_STATUS);
	guint32 status = mrim_package_read_UL(pack);
	purple_debug_info("mrim-prpl", "[%s] Message delivery status: %i\n", __func__, status);
}

int mrim_send_im(PurpleConnection *gc, const char *to, const char *message, PurpleMessageFlags flags) {
	MrimData *mrim = gc->proto_data;
	if (mrim) {
		if (!is_valid_phone((gchar*)to)) {
			purple_debug_info("mrim-prpl", "[%s] Send to buddy '%s' message '%s'\n", __func__, to, message);
			MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE);
			mrim_package_add_UL(pack, 0); /* flags */
			mrim_package_add_LPSA(pack, (gchar*)to);
			{
				gchar *msg = purple_markup_strip_html((gchar*)message);
				mrim_package_add_LPSW(pack, msg);
				g_free(msg);
			}
			mrim_package_add_LPSA(pack, " "); /* TODO: RTF message */
			mrim_add_ack_cb(mrim, pack->header->seq, mrim_message_ack, NULL);
			if (mrim_package_send(pack, mrim)) {
				return 1;
			} else {
				return -E2BIG;
			}
		} else {
			mrim_send_sms(mrim, (gchar*)to, (gchar*)message);
			return 1;
		}
	}
	return -1;
}

unsigned int mrim_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState typing) {
	if (typing == PURPLE_TYPING) {
		purple_debug_info("mrim-prpl", "[%s] Send typing notify to '%s'\n", __func__, name);
		MrimData *mrim = gc->proto_data;
		MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE);
		mrim_package_add_UL(pack, MESSAGE_FLAG_NOTIFY | MESSAGE_FLAG_NORECV);
		mrim_package_add_LPSA(pack, (gchar*)name);
		mrim_package_add_LPSW(pack, " ");
		mrim_package_add_LPSA(pack, " ");
		mrim_package_send(pack, mrim);
		return 9;
	} else {
		return 0;
	}
}

gboolean mrim_send_attention(PurpleConnection  *gc, const char *username, guint type) {
	purple_debug_info("mrim-prpl", "[%s] Send attention to '%s'\n", __func__, username);
	MrimData *mrim = gc->proto_data;
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE);
	mrim_package_add_UL(pack, MESSAGE_FLAG_ALARM | MESSAGE_FLAG_RTF);
	mrim_package_add_LPSA(pack, (gchar*)username);
	mrim_package_add_LPSW(pack, "*WAKEUP*");
	mrim_package_add_LPSA(pack, " ");
	mrim_package_send(pack, mrim);
	return TRUE;
}

void mrim_receive_im(MrimData *mrim, MrimPackage *pack) {
	g_return_if_fail(mrim);
	g_return_if_fail(pack);
	g_return_if_fail(mrim->gc);
	PurpleConnection *gc = mrim->gc;
	guint32 msg_id = mrim_package_read_UL(pack);
	guint32 flags = mrim_package_read_UL(pack);
	gchar *from = mrim_package_read_LPSA(pack);
	gchar *text;
	if (flags & 0x200000) {
		text = mrim_package_read_LPSA(pack);
	} else {
		text = mrim_package_read_LPSW(pack);
	}
	//gchar *formated_text = mrim_package_read_LPSA(pack); /* TODO: RTF message */
	if (!(flags & MESSAGE_FLAG_NORECV)) {
		MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE_RECV);
		mrim_package_add_LPSA(pack, from);
		mrim_package_add_UL(pack, msg_id);
		mrim_package_send(pack, mrim);
	}
	purple_debug_info("mrim-prpl", "[%s] Received from '%s', flags '%x' message '%s'\n", __func__, from, flags, text);
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >5
	gchar *message = purple_markup_escape_text (text, -1);
#else
	gchar *message = g_markup_escape_text(text, -1);
#endif
	if (flags & MESSAGE_FLAG_AUTHORIZE) { /* TODO: Auth message and alias show */
		MrimAuthData *data = g_new0(MrimAuthData, 1);
		data->mrim = mrim;
		data->from = g_strdup(from);
		data->seq = mrim->seq;
		gboolean is_in_blist = (purple_find_buddy(mrim->account, from) != NULL);
		purple_account_request_authorization(mrim->account, from, NULL, NULL, NULL, is_in_blist,
			mrim_authorization_yes, mrim_authorization_no, data);
		return;
	} else if (flags & MESSAGE_FLAG_NOTIFY) {
		serv_got_typing(mrim->gc, from, 10, PURPLE_TYPING);
	} else if (flags & MESSAGE_FLAG_ALARM) {
		serv_got_attention(gc, from, 0);
	} else if (flags & MESSAGE_FLAG_MULTICHAT) {
		mrim_package_read_UL(pack);
		mrim_package_read_UL(pack);
		char *topic = mrim_package_read_LPSA(pack);
		g_free(topic);
		char *from_user = mrim_package_read_LPSW(pack);
		serv_got_chat_in(gc, get_chat_id(from), from_user, PURPLE_MESSAGE_RECV, message, time(NULL));
		purple_debug_info("mrim-prpl", "[%s] This is chat! id '%i'\n", __func__, get_chat_id(from));
	} else {
		serv_got_im(mrim->gc, from, message, PURPLE_MESSAGE_RECV, time(NULL));
	}
	g_free(from);
	g_free(text);
	g_free(message);
}

void mrim_receive_offline_message(MrimData *mrim, gchar *message) {
	purple_debug_info("mrim-prpl", "[%s] Reading offline message\n", __func__);
	gchar *message_header;
	gchar *message_body;
	{
		gchar **split = g_strsplit(message, "\n\r\n", 2);
		message_header = split[0];
		message_body = split[1];
		g_free(split);
	}
	GRegex *regex = g_regex_new("([A-Za-z\\-\\_]+):\\s(.+?)\\R", G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);
	GMatchInfo *match_info;
	g_regex_match(regex, message_header, 0, &match_info);
	gchar *from = NULL;
	gchar *date_str = NULL;
	gchar *boundary = NULL;
	gchar *charset = NULL;
	gchar *encoding = NULL;
	guint32 flags = 0;
	while (g_match_info_matches(match_info)) {
		gchar *key = g_match_info_fetch(match_info, 1);
		gchar *value = g_match_info_fetch(match_info, 2);
		purple_debug_info("mrim-prpl", "[%s] '%s' == '%s'\n", __func__, key, value);
		if (g_strcmp0(key, "From") == 0) {
			from = g_strdup(value);
		} else if (g_strcmp0(key, "Date") == 0) {
			date_str = g_strdup(value);
		} else if (g_strcmp0(key, "Content-Type") == 0) {
			gchar *tmp = g_strrstr(value, "boundary=");
			if (tmp) {
				boundary = (gchar*)(tmp + strlen("boundary="));
				boundary = g_strdup_printf("--%s\r\n", boundary);
			}
			tmp = g_strrstr(value, "charset=");
			if (tmp) {
				charset = g_strdup((gchar*)(tmp + strlen("charset="))); 
			}
		} else if (g_strcmp0(key, "X-Mrim-flags") == 0) {
			sscanf(value, "%x", &flags);
		} else if (g_strcmp0(key, "Content-Transfer-Encoding") == 0) {
			encoding = g_strdup(value);
		}
		g_free(key);
		g_free(value);
		g_match_info_next(match_info, NULL);
	}
	g_match_info_free(match_info);
	g_free(message_header);
	if (boundary) {
		gchar **message_split = g_strsplit(message_body, boundary, 0);
		g_free(message_body);
		g_free(boundary);
		{
			gchar **split = g_strsplit(message_split[1], "\n\r\n", 2);
			message_header = split[0];
			message_body = split[1];
			g_free(split);
		}
		g_strfreev(message_split);
		regex = g_regex_new("([A-Za-z\\-\\_]+):\\s(.+?)\\R", G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);
		g_regex_match(regex, message_header, 0, &match_info);
		while (g_match_info_matches(match_info)) {
			gchar *key = g_match_info_fetch(match_info, 1);
			gchar *value = g_match_info_fetch(match_info, 2);
			purple_debug_info("mrim-prpl", "[%s] '%s' == '%s'\n", __func__, key, value);
			if (g_strcmp0(key, "Content-Transfer-Encoding") == 0) {
				encoding = g_strdup(value);
			} else if (g_strcmp0(key, "Content-Type") == 0) {
				charset = g_strdup((gchar*)(g_strrstr(value, "charset=") + strlen("charset="))); 
			}
			g_free(key);
			g_free(value);
			g_match_info_next(match_info, NULL);
		}
		g_match_info_free(match_info);
		g_free(message_header);
	}
	time_t date = mrim_str_to_time(date_str);
	g_free(date_str);
	if (flags & MESSAGE_FLAG_AUTHORIZE) { /* TODO: Show auth message and alias */
		MrimAuthData *data = g_new0(MrimAuthData, 1);
		data->mrim = mrim;
		data->from = g_strdup(from);
		data->seq = mrim->seq;
		gboolean is_in_blist = (purple_find_buddy(mrim->account,from) != NULL);
		purple_account_request_authorization(mrim->account, from, NULL, NULL, NULL, is_in_blist,
			mrim_authorization_yes, mrim_authorization_no, data);
	} else {
		purple_debug_info("mrim-prpl", "[%s] from == '%s', encoding == '%s', charset == '%s'\n",
			__func__, from, encoding, charset);
		gchar *message_text;
		if (g_strcmp0(encoding, "base64") == 0) {
			gsize len_decoded;
			message_text = (gchar*)purple_base64_decode(message_body, &len_decoded);
			message_text = g_realloc(message_text, len_decoded + 2);
			message_text[len_decoded] = '\0';
			message_text[len_decoded + 1] = '\0';
		} else {
			message_text = g_strdup(message_body);
		}
		if (g_strcmp0(charset, "UTF-16LE") == 0) {
			gchar *str = g_utf16_to_utf8((gunichar2*)message_text, -1, NULL, NULL, NULL);
			g_free(message_text);
			message_text = str;
		}
#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >5
		gchar *msg = purple_markup_escape_text(message_text, -1);
#else
		gchar *msg = g_markup_escape_text(message_text, -1);
#endif
		serv_got_im(mrim->gc, from, msg, PURPLE_MESSAGE_RECV, date);
		g_free(msg);
		g_free(message_text);
	}
	g_free(from);
	g_free(charset);
	g_free(encoding);
	g_free(message_body);
}

void mrim_sms_ack(MrimData *mrim, gpointer user_data, MrimPackage *pack) {
	guint32 status = mrim_package_read_UL(pack);
	purple_debug_info("mrim-prpl", "[%s] Status is %i\n", __func__, status);
	switch (status) {
		case MRIM_SMS_OK:
			purple_notify_info(mrim->gc, _("SMS"), _("SMS message sent."), _("SMS message sent."));
			break;
		case MRIM_SMS_SERVICE_UNAVAILABLE:
			purple_notify_error(mrim->gc, _("SMS"), _("SMS service is not available"), _("SMS service is not available"));
			break;
		case MRIM_SMS_INVALID_PARAMS:
			purple_notify_error(mrim->gc, _("SMS"), _("Wrong SMS settings."), _("Wrong SMS settings."));
			break;
		default:
			purple_notify_error(mrim->gc, _("SMS"), _("Achtung!"), _("Anyone here?? !"));
			break;
	}
}

void mrim_send_sms(MrimData *mrim, gchar *phone, gchar *message) {
	purple_debug_info("mrim-prpl", "[%s] Send SMS to '%s' with text '%s'\n", __func__, phone, message);
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_SMS);
	mrim_package_add_UL(pack, 0);
	mrim_package_add_LPSA(pack, phone);
	mrim_package_add_LPSW(pack, message);
	mrim_add_ack_cb(mrim, pack->header->seq, mrim_sms_ack, NULL);
	mrim_package_send(pack, mrim);
}


/* CHATS */
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
	MrimData *mrim = gc->proto_data;
	const char *username = gc->account->username;
	PurpleConversation *conv = purple_find_chat(gc, id);

	if (conv)
	{
		MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE);
		mrim_package_add_UL(pack, MESSAGE_FLAG_NORECV ); /* flags */
		mrim_package_add_LPSA(pack, conv->name);

		{
			gchar *msg = purple_markup_strip_html((gchar*)message);
			mrim_package_add_LPSW(pack, msg);
			g_free(msg);
		}
		mrim_package_add_LPSA(pack, " "); /* TODO: RTF message */

		serv_got_chat_in(gc, id, mrim->user_name, PURPLE_MESSAGE_SEND, message, time(NULL));
		//mrim_add_ack_cb(mrim, pack->header->seq, mrim_message_ack, NULL);
		if (mrim_package_send(pack, mrim)) {
			return 1;
		} else {
			return -E2BIG;
		}
	}
	else
	{
		purple_debug_info("mrim-prpl", "tried to send message from %s to chat room #%d: %s\n but couldn't find chat room", username, id, message);
		return -EINVAL; // todo why not -1?
	}
}

void mirm_set_chat_topic(PurpleConnection *gc, int id, const char *topic)
{
	purple_debug_info("mrim", "%s\n", __func__);
}
