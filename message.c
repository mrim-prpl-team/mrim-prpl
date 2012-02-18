/*  This file is part of mrim-prpl.
 *
 *  mrim-prpl is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  mrim-prpl is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mrim-prpl.  If not, see <http://www.gnu.org/licenses/>.
 */

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
	mrim_package_add_LPSW(pack, _("User try to wake up you, but you have too old installed version of the Mail.ru Agent."));
	mrim_package_add_LPSA(pack, "eNqNVEtu2zAQFYp2EyB3ELIO7KFEmmS88Am6KbLkRpYpW4giGTKVLAyfqgd0+E3JWv1sNOLwzZs3H+lLlmU/v2XZWYyqQaLqT6191Mc9KggSO9k0YJ5d1e8RYH4WzdArte30C4imbzvR1IdqPElVAM6fq8PwWq0vl/u7s6iHbhg1dC1GuQOxH6XsQWy7SYJ1FYQ4p3kxbm3TixgNKRZuWUO41m29iJRz2YJbW3thzslF4Flh59fWuFf4NiUqmOMoA3np5Gnr0B5OPR7DWvdGvLXy/aXtd1hMNRLHatzZdp4QE/W2FLWexRbEcRyUrJXGTwgI3RjDVtbQ0hnv5PGJOggjzoAzxSa3NqHxJ+7iuYPSwt2VETeC0sVTnGC4Z2Wxl/JYI0eJKohSIYDNYx6XlUgPSYogOfd8f1His7HZikjSQhyXGTr5mfnfbCxpumPjqwhy0zQaB3pSBp/Q71XbLX5MNgT7XGRG3S8d+e0uUD43aJqaRAhlSZvBl17E0wut806crJaOeLCa0Z8H7tpsBj67mw8LVzVN9ojEMgMtn9+jPP4QfCk4CkQQvoC5LfgN8z8DS6E8tB0n7MmOhLlRH8nnZsJDQU/5Qanj03JZ7WWvFq9mOcZpaf4W+f3d5av+dV+v1+wD342mxg==");
	mrim_package_send(pack, mrim);
	return TRUE;
}

void mrim_receive_im_chat(MrimData *mrim, MrimPackage *pack, guint32 msg_id, guint32 flags, gchar *room, gchar *message)
{
	PurpleConnection *gc = mrim->gc;
	gchar *rtf  = mrim_package_read_LPSA(pack); // rtf

	// handle chat-specific functions
	MrimAck *ack = g_hash_table_lookup(mrim->acks, GUINT_TO_POINTER(msg_id));
	if (ack) {
		ack->func(mrim, ack->data, pack);
		g_hash_table_remove(mrim->acks, GUINT_TO_POINTER(msg_id));
		return;
	}
	// just chat message
	mrim_package_read_UL(pack);
	guint32 package_type = mrim_package_read_UL(pack);
	char *topic = mrim_package_read_LPSW(pack);
	char *from_user = mrim_package_read_LPSA(pack);

	PurpleConversation *conv =  purple_find_chat(mrim->gc, get_chat_id(room));
	PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);

	switch (package_type)
	{
		case MULTICHAT_MESSAGE:
			serv_got_chat_in(gc, get_chat_id(room), from_user, PURPLE_MESSAGE_RECV, message, time(NULL));
			purple_debug_info("mrim-prpl", "[%s] This is chat message! id '%i'\n", __func__, get_chat_id(room));
			break;
		case MULTICHAT_ADD_MEMBERS:
			purple_notify_info(gc, "MULTICHAT_ADD_MEMBERS", room, NULL);
			purple_conv_chat_add_user(chat, from_user, message, PURPLE_CBFLAGS_NONE, TRUE);
			break;
		case MULTICHAT_ATTACHED:
			purple_notify_info(gc, "MULTICHAT_ATTACHED", room, NULL);
			break;
		case MULTICHAT_DETACHED:
		{
			purple_conv_chat_remove_user(chat, from_user, NULL);
			break;
		}
		case MULTICHAT_INVITE:
			purple_notify_info(gc, "MULTICHAT_INVITE", room, NULL);
			GHashTable *data = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
			g_hash_table_insert(data, "room", g_strdup(room));
			serv_got_chat_invite(gc, room, from_user, NULL, data);
			break;
		case MULTICHAT_DESTROYED:
			purple_notify_info(gc, "MULTICHAT_DESTROYED", room, NULL);
			purple_conv_chat_remove_user(chat, from_user, message);
			break;
		case MULTICHAT_DEL_MEMBERS:
			purple_notify_info(gc, "MULTICHAT_DEL_MEMBERS", room, NULL);
			purple_conv_chat_remove_user(chat, from_user, message);
			break;
		case MULTICHAT_TURN_OUT:
			purple_notify_info(gc, "MULTICHAT_TURN_OUT", room, NULL);
			// TODO: left from chat
			break;
	}
	g_free(rtf);
	g_free(topic);
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
	if (flags & MESSAGE_FLAG_CP1251) {
		text = mrim_package_read_LPSA(pack);
	} else {
		text = mrim_package_read_LPSW(pack);
	}
	gchar *formated_text = mrim_package_read_LPSA(pack); /* TODO: RTF message */
	if (!(flags & MESSAGE_FLAG_NORECV)) {
		MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE_RECV);
		mrim_package_add_LPSA(pack, from);
		mrim_package_add_UL(pack, msg_id);
		mrim_package_send(pack, mrim);
	}
	purple_debug_info("mrim-prpl", "[%s] Received from '%s', flags 0x%x, message '%s', rtf '%s'\n", __func__, from, flags, text, formated_text);
	gchar *message = purple_markup_escape_text (text, -1);
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
		mrim_receive_im_chat(mrim, pack, msg_id, flags, from, message);
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
	if (purple_account_get_bool(mrim->gc->account, "debug_mode", FALSE)) {
		//serv_got_im(mrim->gc, from, message_body, PURPLE_MESSAGE_RECV, date);
		purple_debug_info("mrim-prpl", "[%s] Unparsed offline message:\n%s\n", __func__, message);
		//return;
	}
	{
		GRegex *regex = g_regex_new("(\n|\r){2}", G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, NULL);
		gchar *message_cleaned = g_regex_replace_literal(regex, message, strlen(message), 0, "\n", 0, NULL);
		gchar **split = g_strsplit(message_cleaned, "\n\n", 2);
		g_free(message_cleaned);
		message_header = g_strconcat(split[0], "\n", NULL);
		g_free(split[0]);
		message_body = split[1];
		g_free(split);
		g_free(regex);
	}
	purple_debug_info("mrim-prpl", "[%s] Unparsed offline message, gonna parse header:\n", __func__);
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
				boundary = g_strdup_printf("--%s\n", boundary);
			}
			tmp = g_strrstr(value, "charset=");
			if (tmp) {
				charset = g_strdup((gchar*)(tmp + strlen("charset="))); 
			}
		} else if (g_ascii_strncasecmp(key, "X-MRIM-Flags", strlen("X-MRIM-Flags")) == 0) {
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
	time_t date = mrim_str_to_time(date_str);
	g_free(date_str);
	if (boundary) {
		purple_debug_info("mrim-prpl", "[%s] Boundary:%s\n", __func__, boundary);
		gchar **message_split = g_strsplit(message_body, boundary, 0);
		g_free(message_body);
		g_free(boundary);
		{
			gchar **split = g_strsplit(message_split[1], "\n\n", 0);
			message_header = g_strconcat(split[0], "\n", NULL);
			g_free(split[0]);
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
	} else {
		purple_debug_info("mrim-prpl", "[%s] No boundary!\n", __func__);
	}
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
		gchar *msg = purple_markup_escape_text(message_text, -1);
		if (purple_account_get_bool(mrim->gc->account, "debug_mode", FALSE)) {
			gchar *dbg_msg = g_strdup_printf("%s {Source='%s'}", msg, message);
			g_free(msg);
			msg = dbg_msg;
		}
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
	purple_debug_info("mrim-prpl", "%s\n", __func__);
	const char *username = gc->account->username;
	PurpleConversation *conv = purple_find_chat(gc, id);
	purple_debug_info("mrim-prpl", "%s receives whisper from %s in chat room %s: %s\n", username, who, conv->name, message);

	/* receive whisper on recipient's account */
	serv_got_chat_in(gc, id, who, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_WHISPER, message, time(NULL));
}

int mrim_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	purple_debug_info("mrim-prpl", "%s\n", __func__);
	MrimData *mrim = gc->proto_data;
	const char *username = gc->account->username;
	PurpleConversation *conv = purple_find_chat(gc, id);

	if (conv == NULL)
	{
		purple_debug_info("mrim-prpl", "tried to send message from %s to chat room %d: %s\n but couldn't find chat room", username, id, message);
		return -EINVAL; // todo why not -1?
	}

	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MESSAGE);
	mrim_package_add_UL(pack, MESSAGE_FLAG_NORECV); /* flags */
	mrim_package_add_LPSA(pack, conv->name);

	gchar *msg = purple_markup_strip_html((gchar*)message);
	mrim_package_add_LPSW(pack, msg);
	g_free(msg);

	mrim_package_add_LPSA(pack, " "); /* TODO: RTF message */

	serv_got_chat_in(gc, id, mrim->user_name, PURPLE_MESSAGE_SEND, message, time(NULL));
	mrim_add_ack_cb(mrim, pack->header->seq, mrim_message_ack, NULL);
	return (mrim_package_send(pack, mrim)) ? 1 : -E2BIG;
}

void mrim_set_chat_topic(PurpleConnection *gc, int id, const char *topic)
{
	purple_debug_info("mrim-prpl", "%s\n", __func__);
}
