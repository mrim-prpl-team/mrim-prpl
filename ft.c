/*
 *  This file is part of mrim-prpl.
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
#include "ft.h"

#include "sys/stat.h"
#include <libgen.h>
#include "stdbool.h"

gboolean mrim_can_send_file(PurpleConnection *gc, const char *who) {
	MrimData *mrim = gc->proto_data;
	g_return_val_if_fail(mrim != NULL, FALSE); // WHY: -1 warning.
	PurpleBuddy *buddy = purple_find_buddy(mrim->account, who);
	MrimBuddy *mb = buddy ? buddy->proto_data : NULL;
	if (mb) {
		return (mb->com_support & FEATURE_FLAG_FILE_TRANSFER);
	} else {
		return FALSE;
	}
}

PurpleXfer *mrim_new_xfer(PurpleConnection *gc, const char *who) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	PurpleXfer *xfer;
	xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);
	g_return_val_if_fail(xfer != NULL, NULL);
	MrimFT *data = g_new0(MrimFT, 1);
	xfer->data = data;
	data->mrim = gc->proto_data;
	data->user_name = g_strdup(who);
	purple_xfer_set_init_fnc(xfer, mrim_xfer_send_rq);
	purple_xfer_set_cancel_send_fnc(xfer, mrim_xfer_cancel);
	return xfer;
}

void mrim_send_file(PurpleConnection *gc, const char *who, const char *file) {
	PurpleXfer *xfer = mrim_new_xfer(gc, who);
	if (file) {
		purple_xfer_request_accepted(xfer, file);
	} else {
		purple_xfer_request(xfer);
	}
}


void mrim_xfer_got_rq(MrimPackage *pack, MrimData *mrim) {
	purple_debug_info("mrim-prpl", "[%s] MRIM_CS_FILE_TRANSFER\n", __func__);
	gchar *user_name = mrim_package_read_LPSA(pack);
	guint32 wtf = mrim_package_read_UL(pack); //Unknown field
	guint32 file_size = mrim_package_read_UL(pack);
	guint32 id = mrim_package_read_UL(pack);
	gchar *file_list = mrim_package_read_LPSA(pack);
	guint32 wtf2 = mrim_package_read_UL(pack); //Unknown field
	gchar *remote_ip = mrim_package_read_LPSA(pack);
	MrimFile *files = NULL;
	guint file_count;
	{
		gchar **parts = g_strsplit(file_list, ";", 0);
		guint i = 0;
		while (parts[i * 2] && parts[i * 2 + 1]) {
			gchar *file_name = g_strdup(parts[i * 2]);
			guint32 f_size = atoi(parts[i * 2 + 1]);
			files = realloc(files, (i + 1) * sizeof(MrimFile));
			files[i].name = file_name;
			files[i].size = f_size;
			i++;
		}
		file_count = i;
		g_strfreev(parts);
	}
	purple_debug_info("mrim-prpl", "[%s] MRIM_CS_FILE_TRANSFER: user_name = '%s', file_size = '%u', file_count = '%u', id='%u'\n", __func__, user_name, file_size, file_count, id);
	g_free(file_list);
	MrimFT *ft = g_new0(MrimFT, 1);
	ft->mrim = mrim;
	ft->user_name = user_name;
	ft->id = id;
	ft->remote_ip = remote_ip;
	ft->files = files;
	ft->count = file_count;
	mrim_process_xfer(ft);
}

void mrim_xfer_ack(MrimPackage *pack, MrimData *mrim) {
	purple_debug_info("mrim-prpl", "[%s] MRIM_CS_FILE_TRANSFER_ACK\n", __func__);
	guint32 status = mrim_package_read_UL(pack);
	gchar *user_name = mrim_package_read_LPSA(pack);
	guint32 id = mrim_package_read_UL(pack);
	gchar *remote_addr = mrim_package_read_LPSA(pack);
	purple_debug_info("mrim-prpl", "[%s] MRIM_CS_FILE_TRANSFER_ACK: status = %u, user_name = '%s', remote_addr = '%s'\n", __func__, status, user_name, remote_addr);
	PurpleXfer *xfer = g_hash_table_lookup(mrim->transfers, GUINT_TO_POINTER(id));
	if (xfer) {
		if (status == FILE_TRANSFER_MIRROR) {
			MrimFT *ft = xfer->data;
			purple_debug_info("mrim-prpl", "[%s] User='%s' accepted files! id='%xu'\n", __func__, user_name, id);
		//Допустим белого IP у нас нет и запросим зеркальный прокси TODO
			MrimPackage *ack = mrim_package_new(mrim->seq++, MRIM_CS_PROXY);
			mrim_package_add_LPSA(ack, user_name);
			mrim_package_add_UL(ack, id);
			mrim_package_add_UL(ack, MRIM_PROXY_TYPE_FILES);
			gchar *file_list = g_strdup_printf("%s;%u;", ft->files[0].name, ft->files[0].size);
			mrim_package_add_LPSA(ack, file_list);
			mrim_package_add_UL(ack, 0); // Много-
			mrim_package_add_UL(ack, 0); // много
			mrim_package_add_UL(ack, 0); // не
			mrim_package_add_UL(ack, 0); // нужных
			mrim_package_add_UL(ack, 0); // полей
			mrim_package_add_UL(ack, 4 + 4 + strlen(file_list) * 2); //Длина последующиъ данных
		// Судя по всему далее идёт повтор пакета, но в UTF16
			mrim_package_add_UL(ack, MRIM_PROXY_TYPE_FILES);
			mrim_package_add_LPSW(ack, file_list);
			mrim_package_add_UL(ack, 4); // Неизвестное поле
			mrim_package_add_UL(ack, 1); // Ещё одно неизвестно поле... Имеет смысл узнать, что это...
			mrim_package_send(ack, mrim);
		} else {
			purple_debug_info("mrim-prpl", "[%s] Transfer cancelled!\n", __func__);
			purple_xfer_unref(xfer);
		}
	}
	g_free(user_name);
	g_free(remote_addr);	
}

void mrim_xfer_proxy_ack(MrimPackage *pack, MrimData *mrim) {
	guint status = mrim_package_read_UL(pack);
	gchar *user_name = mrim_package_read_LPSA(pack);
	guint32 id = mrim_package_read_UL(pack);
	guint32 data_type = mrim_package_read_UL(pack);
	gchar *file_list = mrim_package_read_LPSA(pack);
	gchar *remote_ip = mrim_package_read_LPSA(pack);
	// В пакете есть ещё и другие поля, но они нам не нужны (*кроме proxy_id)
	g_return_if_fail(data_type != MRIM_PROXY_TYPE_FILES);
	PurpleXfer *xfer = g_hash_table_lookup(mrim->transfers, GUINT_TO_POINTER(id));
	if (xfer) {
		if (status == PROXY_STATUS_OK) {
			MrimFT *ft = xfer->data;
			purple_debug_info("mrim-prpl", "[%s] Proxy accepted! Address list = '%s'\n", __func__, remote_ip);
			gchar **addrs = g_strsplit(remote_ip, ";", 0);
			gchar **addr = addrs;
			gchar *ip = NULL;
			guint16 port;
			while (*addr) {
				gchar **parts = g_strsplit(*addr, ":", 2);
				ip = g_strdup(parts[0]);
				port = atoi(parts[1]);
				g_strfreev(parts);
				if (port != 443) { // Мы не умеем SSL
					break;
				} else {
					g_free(ip);
				}
				addr++;
			}
			g_strfreev(addrs);
			purple_debug_info("mrim-prpl", "[%s] Proxy host = '%s', port = %u\n", __func__, ip, port);

			ft->proxy_id[0] = mrim_package_read_UL(pack);
			ft->proxy_id[1] = mrim_package_read_UL(pack);
			ft->proxy_id[2] = mrim_package_read_UL(pack);
			ft->proxy_id[3] = mrim_package_read_UL(pack);

			ft->proxy_conn = purple_proxy_connect(NULL, mrim->account, ip, port, mrim_send_xfer_connect_cb, ft);
		} else {
			purple_debug_info("mrim-prpl", "[%s] Proxy request failed!\n", __func__);
			purple_xfer_unref(xfer);
		}
	}
}

void mrim_xfer_connected(MrimFT *ft) {
	purple_debug_info("mrim-prpl", "[%s] Connected!\n", __func__);
}

void mrim_xfer_connect_cb(gpointer data, gint source, const gchar *error_message) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);

	MrimFT *ft = data;
	ft->proxy_conn = NULL;
	
	if (source >= 0) {
		ft->conn = source;
		mrim_xfer_connected(ft);
	} else {
		//TODO: Connect with proxy
	}
}

void mrim_process_xfer(MrimFT *ft) {
	if (ft->current < ft->count) {
		/* PurpleXfer *xfer = purple_xfer_new(ft->mrim->gc->account, PURPLE_XFER_RECEIVE, ft->user_name);
		purple_xfer_set_filename(xfer, ft->files[ft->current].name);
		purple_xfer_set_size(xfer, ft->files[ft->current].size);
		purple_xfer_request(xfer); */
		//MrimPackage *pack = mrim_package_new(, );
		gchar **parts = g_strsplit(ft->remote_ip, ":", 2);
		//ft->proxy_conn = purple_proxy_connect(ft->mrim->gc, ft->mrim->account, parts[0], parts[1], mrim_xfer_connect_cb, ft);
	} else {
		g_free(ft->user_name);
		g_free(ft->remote_ip);
		guint i;
		for (i = 0; i < ft->count; i++) {
			g_free(ft->files[i].name);
		}
		g_free(ft->files);
		g_free(ft);
	}
}

void mrim_ft_send_input_cb(gpointer data, gint source, PurpleInputCondition cond) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	g_return_if_fail(source >= 0);
	MrimFT *ft = data;
	switch (ft->state)
	{
		case WAITING_FOR_HELLO_ACK:
		{
			MrimPackage *pack = mrim_package_read(ft->fake_mrim);
			if ((pack == NULL) || (pack->header->msg != MRIM_CS_PROXY_HELLO_ACK)) {
				//Что-то пошло не так... надо уходить отсюда...
				purple_debug_info("mrim-prpl", "[%s] Something went wrong... This is FAIL!\n", __func__);
				purple_input_remove(ft->inpa);
				close(ft->conn);
				purple_xfer_unref(ft->xfer);
				mrim_package_free(pack);
				return;
			} else {
				mrim_package_free(pack);
				purple_debug_info("mrim-prpl", "[%s] MRIM_CS_PROXY_HELLO_ACK received!\n", __func__);
				purple_debug_info("mrim-prpl", "[%s] MRIM_CS_PROXY_HELLO_ACK received!\n", __func__);
			}
			//
			g_free(ft->fake_mrim);
			ft->fake_mrim = NULL;
			//
			ft->state = WAITING_FOR_FT_HELLO;
			//
			gchar *buffer = g_strdup_printf("MRA_FT_HELLO %s", ft->mrim->user_name);
			if (send(ft->conn, buffer, strlen(buffer) + 1, 0) >= (strlen(buffer) + 1)) {
				purple_debug_info("mrim-prpl", "[%s] '%s' sent!\n", __func__, buffer);
			} else {
				purple_debug_info("mrim-prpl", "[%s] Failed to send MRA_FT_HELLO!\n", __func__);
				purple_xfer_unref(ft->xfer);
			}
			g_free(buffer);
			break;
		}
		case WAITING_FOR_FT_HELLO:
		{
			// we should read 'MRA_FT_HELLO user\0'
			gchar *buffer		= NULL;
			gchar buf_prev	= NULL;
			guint i = 0;
			do {
				buffer = realloc(buffer, i + 1);
				if (i) {
					buf_prev = buffer[i-1];
				}
				recv(ft->conn, &buffer[i], sizeof(gchar), 0);
				//purple_debug_info("mrim-prpl", "[%s] Received string: %s\n", __func__, buffer);
				i++;
			} while (buffer[i] || buf_prev);
			purple_debug_info("mrim-prpl", "[%s] Received string: %s\n", __func__, buffer);
			
			// wait for 'WAITING_FOR_FT_GET file_name'
			ft->state = WAITING_FOR_FT_GET;
			break;
		}
		case WAITING_FOR_FT_GET:
		{
			purple_debug_info("mrim-prpl", "[%s] FT: WAITING_FOR_FT_GET\n", __func__);
			// we should read 'WAITING_FOR_FT_GET file_name\0'
			gchar *buffer = NULL;
			gchar buf_prev	= NULL;
			guint i = 0;
			do {
				buffer = realloc(buffer, i + 1);
				if (i) {
					buf_prev = buffer[i-1];
				}
				recv(ft->conn, &buffer[i], sizeof(gchar), 0);
				//purple_debug_info("mrim-prpl", "[%s] Received string: %s\n", __func__, buffer);
				i++;
			} while (buffer[i] || buf_prev);
			purple_debug_info("mrim-prpl", "[%s] Received string: %s\n", __func__, buffer);
			ft->state = STREAMING_FT_FILE;
			// TODO
			break;
		}
	}
}

void mrim_send_xfer_connect_cb(gpointer data, gint source, const gchar *error_message) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	MrimFT *ft = data;
	ft->proxy_conn = NULL;
	if (source >= 0) {
		purple_debug_info("mrim-prpl", "[%s] Connected!\n", __func__);
		ft->conn = source;
		ft->state = WAITING_FOR_HELLO_ACK;
		MrimData *mrim  = ft->mrim;
		MrimData *fake_mrim = g_new0(MrimData, 1);
		fake_mrim->fd = source;
		ft->fake_mrim = fake_mrim;
		MrimPackage *pack = mrim_package_new(0, MRIM_CS_PROXY_HELLO);
		pack->header->proto = 0x00010009;
		mrim_package_add_UL(pack, ft->proxy_id[0]);
		mrim_package_add_UL(pack, ft->proxy_id[1]);
		mrim_package_add_UL(pack, ft->proxy_id[2]);
		mrim_package_add_UL(pack, ft->proxy_id[3]);

		if (mrim_package_send(pack, fake_mrim)) {
			ft->inpa = purple_input_add(ft->conn, PURPLE_INPUT_READ, mrim_ft_send_input_cb, ft);
			purple_debug_info("mrim-prpl", "[%s] MRIM_CS_PROXY_HELLO sent!\n", __func__);
		} else {
			purple_debug_info("mrim-prpl", "[%s] Failed to send MRIM_CS_PROXY_HELLO!\n", __func__);
			purple_xfer_unref(ft->xfer);
		}
	} else {
		purple_debug_info("mrim-prpl", "[%s] Fail!\n", __func__);
		purple_xfer_unref(ft->xfer);
	}
}

void mrim_xfer_send_rq(PurpleXfer *xfer) {
	gchar *file_name = purple_xfer_get_local_filename(xfer);
	purple_debug_info("mrim-prpl", "[%s] Sending file '%s'\n", __func__, file_name);
	MrimFT *mrim_ft = xfer->data;
	mrim_ft->xfer = xfer;
	mrim_ft->count = 1;
	mrim_ft->files = g_new0(MrimFT, 1);
	mrim_ft->files[0].name = basename(file_name); // TODO: strdup???
	{
		GRand *rnd = g_rand_new();
		mrim_ft->id = g_rand_int(rnd);
		g_rand_free(rnd);
	}
	{
		struct stat st;
		stat(file_name, &st);
		mrim_ft->files[0].size = st.st_size;
	}
	MrimData *mrim = mrim_ft->mrim;
	MrimPackage *pack = mrim_package_new(mrim->seq++, MRIM_CS_FILE_TRANSFER);
	mrim_package_add_LPSA(pack, mrim_ft->user_name);
	mrim_package_add_UL(pack, mrim_ft->id);
	mrim_package_add_UL(pack, mrim_ft->files[0].size);
	gchar *file_list = g_strdup_printf("%s;%u;", mrim_ft->files[0].name, mrim_ft->files[0].size);
	gchar *my_ip = "1.2.3.4:1234;"; // Заведомо некорректное значение, чтобы клиент обломался и пошёл использовать зеркальный прокси TODO
	mrim_package_add_UL(pack, 4 + strlen(file_list) + 4 + 4 + strlen(my_ip));
	mrim_package_add_LPSA(pack, file_list);
	mrim_package_add_UL(pack, 0);
	mrim_package_add_LPSA(pack, my_ip);
	//mrim_add_ack_cb(mrim, pack->header->seq, mrim_xfer_ack, xfer);
	g_hash_table_insert(mrim->transfers, GUINT_TO_POINTER(mrim_ft->id), xfer);
	mrim_package_send(pack, mrim);
}


void mrim_xfer_cancel(PurpleXfer *xfer) {
	purple_debug_info("mrim-prpl", "[%s]\n", __func__);
	MrimFT *ft = xfer->data;
	if (ft) {
		g_hash_table_remove(ft->mrim->transfers, GUINT_TO_POINTER(ft->id));
	}
}

