#include "mrim.h"
#include "cl.h"
#include "statuses.h"
#include "message.h"
#include "package.h"

/* Plugin loading and unloading */

static void build_default_user_agent() {
	const char *purple_version = purple_core_get_version();
	GHashTable *ht = purple_core_get_ui_info();
	gchar *ui_name = g_hash_table_lookup(ht, "name");
	gchar *ui_version = g_hash_table_lookup(ht, "version");
	mrim_user_agent = g_strdup_printf(
		"client=\"mrim-prpl\" version=\"%s\" build=\"%s\" ui=\"%s %s (libpurple %s)\"",
		DISPLAY_VERSION, BUILD_NUMBER, ui_name, ui_version, purple_version);
}

static gboolean plugin_load(PurplePlugin *plugin) {
	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
	g_free(moods);
	g_free(mrim_user_agent);
	return TRUE;
}

static void plugin_destroy(PurplePlugin *plugin) {
		
}

/* Connection */

static void mrim_balancer_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message);
static void mrim_connect_cb(gpointer data, gint source, const gchar *error_message);
static void mrim_input_cb(gpointer data, gint source, PurpleInputCondition cond);
void free_mrim_ack(MrimAck *ack);

static void mrim_login(PurpleAccount *account) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	g_return_if_fail(account != NULL);
	PurpleConnection *gc = purple_account_get_connection(account);
	g_return_if_fail(gc != NULL);
	
	gc->flags |= PURPLE_CONNECTION_NO_FONTSIZE | PURPLE_CONNECTION_NO_URLDESC | PURPLE_CONNECTION_NO_IMAGES |
		PURPLE_CONNECTION_SUPPORT_MOODS | PURPLE_CONNECTION_SUPPORT_MOOD_MESSAGES;
	
	MrimData *mrim = g_new0(MrimData, 1);
	mrim->account = account;;
	mrim->gc = gc;
	gc->proto_data = mrim;
	
	mrim->user_name = g_strdup(purple_account_get_username(account));
	mrim->password = g_strdup(purple_account_get_password(account));
	
	mrim->fetch_avatars = purple_account_get_bool(account, "fetch_avatars", TRUE);
	mrim->micropost_notify = purple_account_get_bool(account, "micropost_notify", TRUE);
	
	mrim->balancer_host = g_strdup(purple_account_get_string(account, "balancer_host", MRIM_MAIL_RU));
	mrim->balancer_port = purple_account_get_int(account, "balancer_port", MRIM_MAIL_RU_PORT);
	
	if (purple_account_get_bool(account, "use_custom_user_agent", FALSE)) {
		mrim->user_agent = g_strdup(purple_account_get_string(account, "custom_user_agent", NULL));
	} else {
		mrim->user_agent = g_strdup(mrim_user_agent);
	}
	purple_debug_info("mrim-prpl", "[%s] User agent is '%s'\n", __func__, mrim->user_agent);
	
	mrim->groups = g_hash_table_new_full(NULL,NULL, NULL, (GDestroyNotify)free_mrim_group);
	mrim->acks = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)free_mrim_ack);
	
	mrim->status = make_mrim_status_from_purple(purple_presence_get_active_status(account->presence));
	
	purple_connection_update_progress(gc, _("Connecting"), 1, 5);
	
	gchar *balancer_addr = g_strdup_printf("%s:%i", mrim->balancer_host, mrim->balancer_port);
	purple_debug_info("mrim-prpl", "[%s] Balancer address is '%s'\n", __func__, balancer_addr);
	mrim->fetch_url = purple_util_fetch_url_request(balancer_addr, TRUE, NULL, FALSE, NULL, FALSE, mrim_balancer_cb, mrim);
	g_free(balancer_addr);
	
	mrim->use_gtk = TRUE; //TODO: Detect are client supports GTK+
}

static void mrim_close(PurpleConnection *gc) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	
	g_return_if_fail(gc != NULL);
	if (gc->inpa)
	{
		purple_input_remove(gc->inpa);
		gc->inpa = 0;
	}
	MrimData *mrim = gc->proto_data;
	g_return_if_fail(mrim != NULL);
	if (mrim->fetch_url) {
		purple_util_fetch_url_cancel(mrim->fetch_url);
	}
	if (mrim->proxy_connect) {
		purple_proxy_connect_cancel(mrim->proxy_connect);
	}
	if (mrim->keepalive_timeout) {
		purple_timeout_remove(mrim->keepalive_timeout);
	}
	if (mrim->fd >= 0) {
		close(mrim->fd);
	}
	
	free_mrim_status(mrim->status);
	
	g_hash_table_destroy(mrim->groups);
	g_hash_table_destroy(mrim->acks);
	
	g_free(mrim->nick);
	g_free(mrim->microblog);
	
	g_free(mrim->server_host);
	g_free(mrim->user_agent);
	g_free(mrim->balancer_host);
	g_free(mrim->user_name);
	g_free(mrim->password);
	
	g_free(mrim);
	gc->proto_data = NULL;
}

static void mrim_balancer_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);

	MrimData *mrim = user_data;
	g_return_if_fail(mrim != NULL);
	mrim->fetch_url = NULL;

	PurpleConnection *gc = mrim->gc;
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->proto_data != NULL);

	if (len) {
		{
			gchar** split = g_strsplit(url_text, ":", 2);
			mrim->server_host = g_strdup(split[0]);
			mrim->server_port = atoi(split[1]);
			g_strfreev(split);
		}
		purple_debug_info("mrim-prpl", "[%s] MRIM server address is '%s:%i'\n", __func__, mrim->server_host, mrim->server_port);
		purple_connection_update_progress(gc, _("Connecting"), 2, 5);
		mrim->proxy_connect = purple_proxy_connect(mrim->gc, mrim->account, mrim->server_host, mrim->server_port, mrim_connect_cb, mrim->gc);
		if (!mrim->proxy_connect) {
			purple_connection_error_reason(mrim->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to create TCP-connection"));
			purple_debug_error("mrim-prpl", "[%s] Unable to create TCP-connection\n", __func__);
		}
	} else {
		purple_debug_error("mrim-prpl", "[%s] Error: %s\n", __func__, error_message);
		purple_connection_error_reason(mrim->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, error_message);
	}
}

static void mrim_connect_cb(gpointer data, gint source, const gchar *error_message) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	
	PurpleConnection *gc = data;
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->proto_data!= NULL);

	MrimData *mrim = gc->proto_data;
	mrim->proxy_connect = NULL;
	
	if (source >= 0) {
		mrim->fd = source;
		mrim->seq = 0;
		MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_HELLO);
		if (mrim_package_send(pack, mrim)) {
			purple_connection_update_progress(gc, _("Connecting"), 3, 5);
			gc->inpa = purple_input_add(mrim->fd, PURPLE_INPUT_READ, mrim_input_cb, gc);
		} else {
			purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Unable to write to socket."));
			purple_debug_error("mrim-prpl", "[%s] Unable to write to socket\n", __func__);
		}
	} else {
		gchar *msg = g_strdup_printf(_("Unable to connect: %s"), error_message);
		purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, msg);
		purple_debug_error("mrim-prpl", "[%s] %s\n", __func__, msg);
		g_free(msg);
	}
}

gchar *make_mailbox_url(MrimData *mrim, gchar *webkey) {
	gchar *url = NULL;
	if (webkey) {
		url = g_strdup_printf("http://win.mail.ru/cgi-bin/auth?Login=%s&agent=%s", mrim->user_name, webkey);
	} else {
		url = g_strdup_printf("http://win.mail.ru/cgi-bin/auth?Login=%s", mrim->user_name);
	}
	return url;
}

void notify_emails(PurpleConnection *gc, guint count, gchar *webkey) {
	MrimData *mrim = gc->proto_data;
	if (!purple_account_get_check_mail(((PurpleConnection*)gc)->account)) return;
	gchar *url = make_mailbox_url(mrim, webkey);
	gchar **mas = g_new0(gchar*, count);
	gchar **mas_to = g_new(gchar*, count);
	gchar **mas_url = g_new(gchar*, count);
	guint i;
	for (i = 0; i < count; i++) {
		mas_to[i] = mrim->user_name;
		mas_url[i] = url;
	}
	purple_notify_emails(gc, count, FALSE,
		(const char **)mas, (const char**)mas, (const char**)mas_to, (const char**)mas_url, NULL, NULL);
	g_free(mas);
	g_free(mas_to);
	g_free(mas_url);
	g_free(url);
}

void mrim_mpop_session_ack(MrimData *mrim, gpointer user_data, MrimPackage *pack) {
	gchar *webkey = NULL;
	guint status = mrim_package_read_UL(pack);
	if (status == MRIM_GET_SESSION_SUCCESS) {
		webkey = mrim_package_read_LPSA(pack);
		purple_debug_info("mrim-prpl", "[%s] Success. Webkey is '%s'\n", __func__, webkey);
	} else {
		purple_debug_info("mrim-prpl", "[%s] Failed. Status is %i\n", __func__, status);
	}
	MrimNotifyMailData *data = user_data;
	if (data->from || data->subject) {
		gchar *url = make_mailbox_url(mrim, webkey);
		if (purple_account_get_check_mail(mrim->account)) {
			purple_notify_email(mrim->gc, data->subject, data->from, mrim->user_name, url, NULL, NULL);
		}
		g_free(url);
	} else {
		notify_emails(mrim->gc, mrim->mail_count, webkey);
	}
}

static void mrim_input_cb(gpointer data, gint source, PurpleInputCondition cond) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	
	g_return_if_fail(source >= 0);
	PurpleConnection *gc = data;
	g_return_if_fail(gc != NULL);
	g_return_if_fail(gc->proto_data != NULL);
	
	MrimData *mrim = gc->proto_data;
	
	MrimPackage *pack = mrim_package_read(mrim);
	if (pack) {
		mrim->error_count = 0;
		switch (pack->header->msg) {
			case MRIM_CS_HELLO_ACK:
				{
					gc->keepalive = mrim_package_read_UL(pack);
					purple_debug_info("mrim-prpl", "[%s] MRIM_CS_HELLO_ACK: Keep alive period is %i\n", __func__, gc->keepalive);
					if (gc->keepalive) {
						mrim->keepalive_timeout = purple_timeout_add_seconds(gc->keepalive, mrim_keep_alive, gc);
					}
					purple_connection_update_progress(gc, _("Connecting"), 4, 5);
					MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_LOGIN2);
					mrim_package_add_LPSA(pack, mrim->user_name);
					mrim_package_add_LPSA(pack, mrim->password);
					mrim_package_add_UL(pack, mrim->status->id);
					mrim_package_add_LPSA(pack, mrim->status->uri);
					mrim_package_add_LPSW(pack, mrim->status->title);
					mrim_package_add_LPSW(pack, mrim->status->desc);
					mrim_package_add_UL(pack, COM_SUPPORT);
					mrim_package_add_LPSA(pack, mrim->user_agent);
					mrim_package_add_LPSA(pack, "ru");
					mrim_package_add_UL(pack, 0);
					mrim_package_add_UL(pack, 0);
					mrim_package_add_LPSA(pack, " ");
					mrim_package_send(pack, mrim);
					break;
				}
			case MRIM_CS_LOGIN_ACK:
				purple_debug_info("mrim-prpl", "[%s] MRIM_CS_LOGIN_ACK: Logged in\n", __func__);
				purple_connection_set_state(gc, PURPLE_CONNECTED);
				break;
			case MRIM_CS_LOGIN_REJ:
				purple_debug_info("mrim-prpl", "[%s] MRIM_CS_LOGIN_REJ\n", __func__);
				gchar *reason = mrim_package_read_LPSA(pack);
				purple_connection_error_reason (gc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED, reason);
				g_free(reason);
				break;
			case MRIM_CS_LOGOUT:
				{
					guint32 reason = mrim_package_read_UL(pack);
					purple_debug_info("mrim-prpl", "[%s] MRIM_CS_LOGOUT: reason = %i\n", __func__, reason);
					if (reason == LOGOUT_NO_RELOGIN_FLAG) {
						purple_connection_error_reason (gc, PURPLE_CONNECTION_ERROR_NAME_IN_USE,
							_("Logged in from another location."));
					} else { 
						purple_connection_error_reason (gc, PURPLE_CONNECTION_ERROR_OTHER_ERROR , _("Server broke connection."));
					}				
					break;
				}
			case MRIM_CS_CONNECTION_PARAMS:
				{
					gc->keepalive = mrim_package_read_UL(pack);
					purple_debug_info("mrim-prpl","[%s] MRIM_CS_CONNECTION_PARAMS: Keep alive period is %i\n",
						__func__, gc->keepalive);
					if (mrim->keepalive_timeout) {
						purple_timeout_remove(mrim->keepalive_timeout);
					}
					if (gc->keepalive) {
						mrim->keepalive_timeout = purple_timeout_add_seconds(gc->keepalive,mrim_keep_alive,gc);
					}
					break;
				}
			case MRIM_CS_CONTACT_LIST2:
				{
					switch (mrim_package_read_UL(pack)) {
						case GET_CONTACTS_OK:
							purple_debug_info("mrim-prpl", "[%s] MRIM_CS_CONTACT_LIST: OK\n", __func__);
							mrim_cl_load(pack, mrim);
							break;
						case GET_CONTACTS_ERROR:
							purple_debug_info("mrim-prpl", "[%s] MRIM_CS_CONTACT_LIST: GET_CONTACTS_ERROR\n", __func__);
							break;
						case GET_CONTACTS_INTERR:
							purple_debug_info("mrim-prpl", "[%s] MRIM_CS_CONTACT_LIST: GET_CONTACTS_INTERR\n", __func__);
							break;
						default:
							purple_debug_info("mrim-prpl", "[%s] MRIM_CS_CONTACT_LIST: Unknown result code\n", __func__);
							break;
					}
					break;
				}
			case MRIM_CS_USER_STATUS:
				{
					guint32 id = mrim_package_read_UL(pack);
					gchar *uri = mrim_package_read_LPSA(pack);
					gchar *tmp = mrim_package_read_LPSW(pack);
					gchar *title = purple_markup_escape_text(tmp, -1);
					g_free(tmp);
					tmp = mrim_package_read_LPSW(pack);
					gchar *desc = purple_markup_escape_text(tmp, -1);
					g_free(tmp);
					gchar *email = mrim_package_read_LPSA(pack);
					mrim_package_read_UL(pack);
					gchar *user_agent = mrim_package_read_LPSA(pack);
					PurpleBuddy *buddy = purple_find_buddy(mrim->account, email);
					if (buddy && buddy->proto_data) {
						MrimBuddy *mb = buddy->proto_data;
						g_free(mb->user_agent);
						mb->user_agent = user_agent;
						free_mrim_status(mb->status);
						mb->status = make_mrim_status(id, uri, title, desc);
						update_buddy_status(buddy);
						purple_debug_info("mrim-prpl", "[%s] MRIM_CS_USER_STATUS: email = '%s', status = '%s'\n", __func__, email,
							mb->status->purple_id);
					} else {
						purple_debug_info("mrim-prpl", "[%s] MRIM_CS_USER_STATUS: Unknown email '%s'!\n", __func__, email);
						g_free(uri);
						g_free(title);
						g_free(desc);
						g_free(user_agent);
					}
					g_free(email);
					break;
				}
			case MRIM_CS_MICROBLOG_RECV:
				{
					guint32 flags = mrim_package_read_UL(pack);
					gchar *email = mrim_package_read_LPSA(pack);
					mrim_package_read_UL(pack);
					mrim_package_read_UL(pack);
					mrim_package_read_UL(pack);
					gchar *tmp = mrim_package_read_LPSW(pack);
					gchar *microblog = purple_markup_escape_text(tmp, -1);
					g_free(tmp);
					mrim_package_read_UL(pack);
					purple_debug_info("mrim-prpl", "[%s] MRIM_CS_MICROBLOG_REСV: email = '%s', microblog = '%s'\n", __func__,
						email, microblog);
					PurpleBuddy *buddy = purple_find_buddy(mrim->account, email);
					if (buddy) {
						set_buddy_microblog(mrim, buddy, microblog, flags);
					}
					g_free(email);
					g_free(microblog);
					break;
				}
			case MRIM_CS_MESSAGE_ACK:
				{
					purple_debug_info("mrim-prpl", "[%s] MRIM_CS_MESSAGE_ACK\n", __func__);
					mrim_receive_im(mrim, pack);
					break;
				}
			case MRIM_CS_OFFLINE_MESSAGE_ACK:
				{
					purple_debug_info("mrim-prpl", "[%s] MRIM_CS_OFFLINE_MESSAGE_ACK\n", __func__);
					gchar *uidl = mrim_package_read_UIDL(pack);
					gchar *message = mrim_package_read_LPSA(pack);
					mrim_receive_offline_message(mrim, message);
					g_free(message);
					MrimPackage *pack_ack = mrim_package_new(mrim->seq++, MRIM_CS_DELETE_OFFLINE_MESSAGE);
					mrim_package_add_UIDL(pack_ack, uidl);
					mrim_package_send(pack_ack, mrim);
					g_free(uidl);
					break;
				}
			case MRIM_CS_USER_INFO:
				{
					gchar *key, *value;
					purple_debug_info("mrim-prpl", "[%s] MRIM_CS_USER_INFO\n", __func__);
					while (pack->cur < pack->data_size) {
						key = mrim_package_read_LPSA(pack);
						value = mrim_package_read_LPSW(pack);
						purple_debug_info("mrim-prpl", "[%s] User info: '%s' = '%s'\n", __func__, key, value);
						if (g_strcmp0(key, "micblog.status.text") == 0) {
							mrim->microblog = value;
						} else if (g_strcmp0(key, "MRIM.NICKNAME") == 0) {
							mrim->nick = value;
						} else if (g_strcmp0(key, "MESSAGES.UNREAD") == 0) {
							mrim->mail_count = atoi(value);
							g_free(value);
							if (mrim->mail_count) {
								MrimNotifyMailData *data = g_new0(MrimNotifyMailData, 1);
								data->count = mrim->mail_count;
								MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_GET_MPOP_SESSION);
								mrim_add_ack_cb(mrim, pack->header->seq, mrim_mpop_session_ack, data);
								mrim_package_send(pack, mrim);
							}
						} else {
							g_free(value);
						}
						g_free(key);
					}
					purple_debug_info("mrim-prpl", "[%s] User nick is '%s'\n", __func__, mrim->nick);
					purple_debug_info("mrim-prpl", "[%s] User microblog is '%s'\n", __func__, mrim->microblog);
				}
				break;
			case MRIM_CS_MAILBOX_STATUS:
				mrim->mail_count = mrim_package_read_UL(pack);
				purple_debug_info("mrim-prpl", "[%s] MRIM_CS_MAILBOX_STATUS: Mail count is %i\n",
					__func__, mrim->mail_count);
				if (mrim->mail_count) {
					MrimNotifyMailData *data = g_new0(MrimNotifyMailData, 1);
					data->count = mrim->mail_count;
					MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_GET_MPOP_SESSION);
					mrim_add_ack_cb(mrim, pack->header->seq, mrim_mpop_session_ack, data);
					mrim_package_send(pack, mrim);
				}
				break;
			case MRIM_CS_NEW_MAIL:
				mrim->mail_count += mrim_package_read_UL(pack); // ??
				if (mrim->mail_count) {
					MrimNotifyMailData *data = g_new0(MrimNotifyMailData, 1);
					data->count = mrim->mail_count;
					data->from = mrim_package_read_LPSA(pack);
					data->subject = mrim_package_read_LPSA(pack);
					purple_debug_info("mrim-prpl",
						"[%s] MRIM_CS_NEW_MAIL: Mail count is %i, from is %s, subject is %s\n",
						__func__, mrim->mail_count, data->from, data->subject);
					MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_GET_MPOP_SESSION);
					mrim_add_ack_cb(mrim, pack->header->seq, mrim_mpop_session_ack, data);
					mrim_package_send(pack, mrim);
				}
				break;
			default:
				{
					MrimAck *ack = g_hash_table_lookup(mrim->acks,
						GUINT_TO_POINTER(pack->header->seq));
					if (ack) {
						ack->func(mrim, ack->data, pack);
						g_hash_table_remove(mrim->acks, GUINT_TO_POINTER(pack->header->seq));
					} else {
						purple_debug_info("mrim-prpl", "[%s] Unknown package received: type = 0x%x, len = %i\n", __func__,
							pack->header->msg, (guint)pack->data_size);
					}
					break;
				}
		}
		mrim_package_free(pack);
	} else {
		int err;
		if (purple_input_get_error(mrim->fd, &err) != 0) {
			purple_debug_info("mrim-prpl", "[%s] Input error", __func__);
			purple_connection_error_reason (gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Input Error"));
		}
		mrim->error_count++;
		if (mrim->error_count > MRIM_MAX_ERROR_COUNT) {
			purple_debug_info("mrim-prpl", "[%s] Bad package\n", __func__);
			purple_connection_error_reason (gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, _("Bad Package"));
		}
	}
}

static gboolean mrim_keep_alive(gpointer data) {
	g_return_val_if_fail(data != NULL, FALSE);
	PurpleConnection *gc = data;
	g_return_val_if_fail(gc->state != PURPLE_DISCONNECTED, FALSE);
	MrimData *mrim = gc->proto_data;
	purple_debug_info("mrim-prpl", "[%s] Sending keep alive #%u\n", __func__, mrim->seq);
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_PING);
	mrim_package_send(pack, mrim);
	return TRUE;
}

void free_mrim_ack(MrimAck *ack) {
	g_free(ack->data);
	g_free(ack);
}

void mrim_add_ack_cb(MrimData *mrim, guint32 seq, void (*func)(MrimData *, gpointer, MrimPackage *), gpointer data) {
	MrimAck *ack = g_new(MrimAck, 1);
	ack->seq = seq;
	ack->func = func;
	ack->data = data;
	g_hash_table_insert(mrim->acks, GUINT_TO_POINTER(seq), ack);
}

/* Actions */

void mrim_search(PurpleConnection *gc, PurpleRequestFields *fields) {
	g_return_if_fail(gc);
	MrimData *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_WP_REQUEST);
	gchar *const_str, *str;
	const_str = (gchar*)purple_request_fields_get_string(fields, "text_box_nickname");
	str = g_strstrip(const_str);
	if (str && str[0]) {
		mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_NICKNAME);
		mrim_package_add_LPSW(pack, str);
	}
	const_str = (gchar*)purple_request_fields_get_string(fields, "text_box_first_name");
	str = g_strstrip(const_str);
	if (str && str[0]) {
		mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_FIRSTNAME);
		mrim_package_add_LPSW(pack, str);
	}
	const_str = (gchar*)purple_request_fields_get_string(fields, "text_box_last_name");
	str = g_strstrip(const_str);
	if (str && str[0]) {
		mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_FIRSTNAME);
		mrim_package_add_LPSW(pack, str);
	}
	const_str = (gchar*)purple_request_fields_get_string(fields, "text_box_age_from");
	str = g_strstrip(const_str);
	if (str && str[0]) {
		mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_DATE1);
		mrim_package_add_LPSW(pack, str);
	}
	const_str = (gchar*)purple_request_fields_get_string(fields, "text_box_age_to");
	str = g_strstrip(const_str);
	if (str && str[0]) {
		mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_DATE2);
		mrim_package_add_LPSW(pack, str);
	}
	{
		PurpleRequestField *field = purple_request_fields_get_field(fields, "radio_button_gender");
		if (field->u.choice.value) {
			mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_SEX);
			mrim_package_add_LPSW(pack, field->u.choice.value == 1 ? "1" : "2");
		}
		field = purple_request_fields_get_field(fields, "check_box_online");
		if (field->u.boolean.value) {
			mrim_package_add_UL(pack, MRIM_CS_WP_REQUEST_PARAM_ONLINE);
			mrim_package_add_LPSW(pack, "1");
		}
	}
	mrim_add_ack_cb(mrim, pack->header->seq, mrim_search_ack, NULL);
	mrim_package_send(pack, mrim);
}

void mrim_search_action(PurplePluginAction *action) {
	PurpleConnection *gc = (PurpleConnection*)action->context;
	MrimData *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	purple_debug_info("mrim-prpl","[%s]\n",__func__);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_string_new("text_box_nickname", _("Nickname"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_first_name",_("FirstName"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_last_name",_("LastName"),"",FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_choice_new("radio_button_gender", _("Sex"), 0);
	purple_request_field_choice_add(field, _("No matter")); // TODO maybe, purple_request_field_list* ?
	purple_request_field_choice_add(field, _("Male"));
	purple_request_field_choice_add(field, _("Female"));
	purple_request_field_group_add_field(group, field);
	/* country */
	/* region */
	/* city */
	/* birthday */
	/* zodiak */
	field = purple_request_field_string_new("text_box_age_from", _("Age from"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("text_box_age_to", _("Age up to"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	/* webkam */
	/* gotov poboltat' */
	field = purple_request_field_bool_new("check_box_online", _("Online"), FALSE);
	purple_request_field_group_add_field(group, field);
	purple_request_fields(mrim->gc, _("Buddies search"), NULL, NULL, fields,
			_("_Search"), G_CALLBACK(mrim_search),
			_("_Cancel"), NULL,
			mrim->account, mrim->user_name, NULL, mrim->gc);
}

void mrim_myworld_action(PurplePluginAction *action) {
	PurpleConnection *gc = (PurpleConnection*)action->context;
	MrimData *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	mrim_open_myworld_url(mrim->user_name, action->user_data);
}

void mrim_post_microblog_record(MrimData *mrim, gchar *message) {
	purple_debug_info("mrim-prpl", "[%s] Micropost is '%s'\n", __func__, message);
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_MICROBLOG_POST);
	mrim_package_add_UL(pack, 9);
	mrim_package_add_LPSW(pack, message);
	mrim_package_send(pack, mrim);
	g_free(mrim->microblog);
	mrim->microblog = g_strdup(message);
}

void mrim_post_microblog_submit(PurpleConnection *gc, PurpleRequestFields *fields) {
	g_return_if_fail(gc);
	MrimData *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	gchar *message = (gchar*)purple_request_fields_get_string(fields, "text_box_micropost");
	mrim_post_microblog_record(mrim, message);
}

void mrim_microblog_action(PurplePluginAction *action) {
	PurpleConnection *gc = (PurpleConnection*)action->context;
	MrimData *mrim = gc->proto_data;
	g_return_if_fail(mrim);
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);
	field = purple_request_field_string_new("text_box_micropost", _("Micropost"), "", FALSE);
	purple_request_field_group_add_field(group, field);
	purple_request_fields(mrim->gc, _("Microblog"), NULL, NULL, fields,
			_("_Post"), G_CALLBACK(mrim_post_microblog_submit),
			_("_Cancel"), NULL,
			mrim->account, mrim->user_name, NULL, mrim->gc);
}

GList *mrim_prpl_actions(PurplePlugin *plugin, gpointer context) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	GList *actions = NULL;
	PurplePluginAction *action;
	action = purple_plugin_action_new(_("Post microblog record..."), mrim_microblog_action);
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("Search for buddies..."), mrim_search_action);
	actions = g_list_append(actions, action);
	actions = g_list_append(actions, NULL);
	action = purple_plugin_action_new(_("MyWorld@Mail.ru"), mrim_myworld_action);
	action->user_data = "http://r.mail.ru/cln3587/my.mail.ru/%s/%s";
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("Photo@Mail.ru"), mrim_myworld_action);
	action->user_data = "http://r.mail.ru/cln3565/foto.mail.ru/%s/%s";
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("Video@Mail.ru"), mrim_myworld_action);
	action->user_data = "http://r.mail.ru/cln3567/video.mail.ru/%s/%s";
	actions = g_list_append(actions, action);
	action = purple_plugin_action_new(_("Blogs@Mail.ru"), mrim_myworld_action);
	action->user_data = "http://r.mail.ru/cln3566/blogs.mail.ru/%s/%s";
	actions = g_list_append(actions, action);
	return actions;
}

/* Some other functions */

static gboolean mrim_offline_message(const PurpleBuddy *buddy) {
	return TRUE;
}

const char *mrim_list_emblem(PurpleBuddy *buddy) {
	g_return_val_if_fail(buddy, NULL);
	MrimBuddy *mb = purple_buddy_get_protocol_data(buddy);
	if (mb) {
		if (!mb->authorized) {
			return "not-authorized";
		}
	}
	return NULL;
}

static const char *mrim_list_icon(PurpleAccount *account, PurpleBuddy *buddy) {
	return "mrim";
}

void mrim_open_myworld_url(gchar *user_name, gchar *fmt) {
	gchar **split = g_strsplit(user_name, "@", 2);
	if (split[1]) { /* domain */
		gsize len = strlen(split[1]);
		if (len > 3) {
			split[1][len - 3] = 0; //Убираем .ru
		}
	}
	gchar *url = g_strdup_printf(fmt, split[1], split[0]);
	g_strfreev(split);
	purple_notify_uri(mrim_plugin, url);
	g_free(url);
}

/* Purple plugin structures */

static PurplePluginProtocolInfo prpl_info = {
	OPT_PROTO_MAIL_CHECK | OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME, /* options */
	NULL,			/* user_splits */
	NULL,			/* protocol_options */
	{			/* icon_spec */
		"png,gif,jpg",			/* format */
		0,				/* min_width */
		0,				/* min_height */
		128,				/* max_width */
		128,				/* max_height */
		10240,				/* max_filesize */
		PURPLE_ICON_SCALE_DISPLAY	/* scale_rules */
	},
	mrim_list_icon,		/* list_icon */
	mrim_list_emblem,	/* list_emblem */
	mrim_status_text,	/* status_text */
	mrim_tooltip_text,	/* tooltip_text */
	mrim_status_types,	/* status_types */
	mrim_user_actions,	/* user_actions */
	NULL,			/* chat_info */
	NULL,			/* chat_info_defaults */
	mrim_login,		/* login */
	mrim_close,		/* close */
	mrim_send_im,		/* send_im */
	NULL,			/* set_info */
	mrim_send_typing,	/* send_typing */
	mrim_get_info,		/* get_info */
	mrim_set_status,	/* set_status */
	NULL,			/* set_idle */
	NULL,			/* change_password */
	mrim_add_buddy,		/* add_buddy */
	NULL,			/* add_buddies */
	mrim_remove_buddy,	/* remove_buddy */
	NULL,			/* remove_buddies */
	NULL,			/* add_permit */
	NULL,			/* add_deny */
	NULL,			/* rem_permit */
	NULL,			/* rem_deny */
	NULL,			/* set_permit_deny */
	NULL,			/* chat_join */
	NULL,			/* reject_chat */
	NULL,			/* get_chat_name */
	NULL,			/* chat_invite */
	NULL,			/* chat_leave */
	NULL,			/* whisper */
	NULL,			/* chat_send */
	NULL,			/* keep_alive */
	NULL,			/* register_user */
	NULL,			/* get_cb_info */
	NULL,			/* get_cb_away */
	mrim_alias_buddy,	/* alias_buddy */
	mrim_move_buddy,	/* move_buddy */
	mrim_rename_group,	/* rename_group */
	mrim_free_buddy,	/* free_buddy */
	NULL,			/* convo_closed */
	mrim_normalize,		/* normalize */
	NULL,			/* set_buddy_icon */
	mrim_remove_group,	/* remove_group */
	NULL,			/* get_cb_real_name */
	NULL,			/* set_chat_topic */
	NULL,			/* find_blist_chat */
	NULL,			/* roomlist_get_list */
	NULL,			/* roomlist_cancel */
	NULL,			/* roomlist_expand_category */
	NULL,			/* can_receive_file */
	NULL,			/* send_file */
	NULL,			/* new_xfer */
	mrim_offline_message,	/* mrim_offline_message */
	NULL,			/* whiteboard_prpl_ops */
	NULL,			/* send_raw */
	NULL,			/* roomlist_room_serialize */
	NULL,			/* unregister_user */
	mrim_send_attention,	/* send_attention */
	NULL,			/* get_attention_types */
	#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >=5
		sizeof(PurplePluginProtocolInfo),	/* struct_size */
	#else
		(gpointer)sizeof(PurplePluginProtocolInfo),
	#endif

	#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 5
		NULL,					/* get_account_text_table */
	#endif
	#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 6
		NULL,					/* initiate_media */
		NULL,					/* get_media_caps */
	#endif
	#if PURPLE_MAJOR_VERSION >= 2 && PURPLE_MINOR_VERSION >= 7
		mrim_get_moods,				/* get_moods */
		NULL,					/* set_public_alias */
		NULL					/* get_public_alias */
	#endif
};

static PurplePluginInfo plugin_info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"prpl-ostin-mrim-experimental",
	"Mail.ru Agent (experimental)",
	DISPLAY_VERSION,
	"Mail.ru Agent protocol plugin",
	"Mail.ru Agent protocol plugin",
	"My Name <email@helloworld.tld>",
	"http://code.google.com/p/mrim-prpl/",     
	plugin_load,		/* plugin_load */                
	plugin_unload,		/* plugin_unload */
	plugin_destroy,		/* plugin_destroy */
	NULL,
	&prpl_info,		/* extra_info */
	NULL,                        
	mrim_prpl_actions,	/* plugin_actions */                   
	NULL,                          
	NULL,                          
	NULL,                          
	NULL                           
};

static void init_plugin(PurplePlugin *plugin) {
	purple_debug_info("mrim-prpl", "Starting up\n");
	generate_mood_list();
	build_default_user_agent();
	PurpleAccountOption *option = purple_account_option_string_new(_("Server"), "balancer_host", MRIM_MAIL_RU);
	prpl_info.protocol_options = g_list_append(NULL, option);
	option = purple_account_option_int_new(_("Port"), "balancer_port", MRIM_MAIL_RU_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Load userpics"), "fetch_avatars", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Micropost notify"), "micropost_notify", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_bool_new(_("Use custom user agent string"), "use_custom_user_agent", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = purple_account_option_string_new(_("Custom user agent"), "custom_user_agent", mrim_user_agent);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	mrim_plugin = plugin;
}

PURPLE_INIT_PLUGIN(mrim_prpl, init_plugin, plugin_info)
