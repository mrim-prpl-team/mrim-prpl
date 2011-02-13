/**   filetransfer.c of mrim-prpl project.
* File Transfer routines.
* Committed by ostinru@gmail.com aka Ostin.
* Thanks to bounjur plugin developers.
*/
#include "filetransfer.h"

/* Look for specific xfer handle */
static unsigned int next_id = 0;

void mrim_xfer_init(PurpleXfer *xfer);
void mrim_xfer_start( PurpleXfer* xfer );
void mrim_xfer_cancel_send(PurpleXfer *xfer);
void mrim_xfer_end(PurpleXfer *xfer);
void mrim_xfer_free(PurpleXfer *xfer);

// открывает listener
void mrim_xfer_slave_init(PurpleXfer *xfer, const gchar *to);
// открывает tcp client
void mrim_xfer_master_init(PurpleXfer *xfer, char *from);


/*
 * Вызывается при отправке юзером файла
 */
void mrim_send_file(PurpleConnection *gc, const char *who, const char *filename)
{
	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);
	purple_debug_info("mrim", "[%s] to <%s>\n", __func__, who);

	PurpleXfer *xfer = mrim_xfer_new(gc, who);

	if (filename)
		purple_xfer_request_accepted(xfer, filename);
	else
		purple_xfer_request(xfer); // пользователь должен выбрать имя файла(и путь) к файлу
}


PurpleXfer *mrim_xfer_new(PurpleConnection *gc, const char *who)
{
	PurpleXfer *xfer;
	XepXfer *xf;
	mrim_data *mrim;

	if(who == NULL || gc == NULL)
		return NULL;

	purple_debug_info("mrim", "[%s] to <%s>\n", __func__, who);
	mrim = (mrim_data*) gc->proto_data;
	if(mrim == NULL)
		return NULL;

	/* Build the file transfer handle */
	xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);
	xfer->data = xf = g_new0(XepXfer, 1);
	xf->mrim = mrim;

	purple_xfer_set_init_fnc(xfer, mrim_xfer_init);
	purple_xfer_set_start_fnc( xfer, mrim_xfer_start );
	purple_xfer_set_cancel_send_fnc(xfer, mrim_xfer_cancel_send);
	purple_xfer_set_end_fnc(xfer, mrim_xfer_end);
//	purple_xfer_set_write_fnc( xfer, mxit_xfer_write );

	mrim->xfer_lists = g_list_append(mrim->xfer_lists, xfer);

	return xfer;
}


/*
 * Эта функция будет вызвана, когда пользователь согласится на приём/передачу
 * файла
 */
void mrim_xfer_init(PurpleXfer *xfer)
{
	PurpleBuddy *buddy;
	mrim_buddy *mb;
	XepXfer *xf;

	xf = (XepXfer*)xfer->data;
	if(xf == NULL)
		return;

	purple_debug_info("mrim", "[%s]\n",__func__);

	buddy = purple_find_buddy(xfer->account, xfer->who);
//	/* this buddy is offline. */
//	if (buddy == NULL || (mb = purple_buddy_get_protocol_data(buddy)) == NULL)
//		return;

	/* Assume it is the first IP. We could do something like keep track of which one is in use or something. */
	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND)
	{	// ОТПРАВКА
		if ( purple_xfer_get_size( xfer ) > MRIM_MAX_FILESIZE ) {
			/* the file is too big */
			purple_xfer_error( xfer->type, xfer->account, xfer->who, _("The file you are trying to send is too large!") );
			purple_xfer_cancel_local( xfer );
			return;
		}
		purple_debug_info("mrim", "mrim xfer type is PURPLE_XFER_SEND.\n");

		//purple_xfer_start(xfer, -1, NULL, 0);
		mrim_xfer_slave_init(xfer, xfer->who);
	}
	else
	{	// ПРИЁМ
		purple_debug_info("mrim", "mrim xfer type is PURPLE_XFER_RECEIVE.\n");
		mrim_xfer_master_init(xfer, xfer->who);
	}

	purple_debug_info("mrim","[%s] bytes_remaining=<%i> \n",__func__, xfer->bytes_remaining);
}

void mrim_xfer_cancel_send(PurpleXfer *xfer)
{
	// TODO разорвать соединение???
	purple_debug_info("mrim", "[%s]\n",__func__);
	mrim_xfer_free(xfer);
}


void mrim_xfer_end(PurpleXfer *xfer)
{
	purple_debug_info("mrim", "[%s]\n",__func__);

	/* We can't allow the server side to close the connection until the client is complete,
	 * otherwise there is a RST resulting in an error on the client side */
/*	if (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND && purple_xfer_is_completed(xfer)) {
		struct socket_cleanup *sc = g_new0(struct socket_cleanup, 1);
		sc->fd = xfer->fd;
		xfer->fd = -1;
		sc->handle = purple_input_add(sc->fd, PURPLE_INPUT_READ, _wait_for_socket_close, sc);
	}
*/
	mrim_xfer_free(xfer);
}

void mrim_xfer_free(PurpleXfer *xfer)
{
	XepXfer *xf;

	g_return_if_fail(xfer != NULL);

	purple_debug_info("mrim", "[%s] %p\n",__func__, xfer);

	xf = (XepXfer*)xfer->data;
	if(xf != NULL)
	{
		mrim_data *mrim = xf->mrim;
		/*if(mrim != NULL)
		{
			mrim->xfer_lists = g_list_remove(mrim->xfer_lists, xfer);
			purple_debug_info("mrim", "B free xfer from lists(%p).\n", mrim->xfer_lists);
		}
		if (xf->proxy_connection != NULL)
			purple_proxy_connect_cancel(xf->proxy_connection);
		if (xf->proxy_info != NULL)
			purple_proxy_info_destroy(xf->proxy_info);
		if (xf->listen_data != NULL)
			purple_network_listen_cancel(xf->listen_data);
		g_free(xf->proxy_host);
		g_free(xf->buddy_ip);
		g_free(xf->sid);
		*/
		g_free(xf);

		xfer->data = NULL;
	}
	purple_debug_info("mrim", "Need close socket=%d.\n", xfer->fd);
}


void mrim_xfer_start( PurpleXfer* xfer )
{
	unsigned char*	buffer;
	int				size;
	int				wrote;

	purple_debug_info("mrim", "[%s]\n", __func__);

	if ( purple_xfer_get_type( xfer ) == PURPLE_XFER_SEND ) {
		/*
		 * the user wants to send a file to one of his contacts. we need to create
		 * a buffer and copy the file data into memory and then we can send it to
		 * the contact. we will send the whole file with one go.

		buffer = g_malloc( xfer->bytes_remaining );
		size = fread( buffer, xfer->bytes_remaining, 1, xfer->dest_fp );

		wrote = purple_xfer_write( xfer, buffer, xfer->bytes_remaining );
		if ( wrote > 0 )
			purple_xfer_set_bytes_sent( xfer, wrote );

		// free the buffer
		g_free( buffer );
		buffer = NULL;
		*/
		purple_debug_info("mrim","[%s] status=<%u>  bytes_remaining=<%i> \n",__func__, xfer->status, xfer->bytes_remaining);

	}
}


void mrim_xfer_denied(PurpleXfer *xfer)
{
	purple_debug_info("mrim","[%s] status=<%u>\n",__func__, xfer->status);

}

//////////////////////////

void mrim_xfer_slave_init(PurpleXfer *xfer, const gchar *to)
{
	xmlnode *si_node, *feature, *field, *file, *x;
	XepXfer *xf = xfer->data;
	mrim_data *mrim = NULL;
	char buf[32];

	if(!xf)
		return;

	mrim = xf->mrim;
	if(!mrim)
		return;

	purple_debug_info("mrim", "[%s] status=<%u>\n", __func__, xfer->status);
	purple_debug_info("mrim", "xep file transfer stream initialization offer-id=%d.\n", next_id);
	/*Construct Stream initialization offer message.*/
	// TODO


	if ( purple_xfer_get_type( xfer ) == PURPLE_XFER_SEND )
	{
		/*
		 * the user wants to send a file to one of his contacts. we need to create
		 * a buffer and copy the file data into memory and then we can send it to
		 * the contact. we will send the whole file with one go.
		 */
		purple_debug_info("mrim","[%s] bytes_remaining=<%i> \n",__func__, xfer->bytes_remaining);
		/*unsigned char *buffer = g_malloc( xfer->bytes_remaining );
		int size = fread( buffer, xfer->bytes_remaining, 1, xfer->dest_fp );

		int wrote = purple_xfer_write( xfer, buffer, xfer->bytes_remaining );
		if ( wrote > 0 )
			purple_xfer_set_bytes_sent( xfer, wrote );

		// free the buffer
		g_free( buffer );
		buffer = NULL;*/
		//purple_xfer_update_progress
	}
}

void mrim_xfer_master_init(PurpleXfer *xfer, char *from)
{
	XepXfer *xf;
	mrim_data *mrim;

	g_return_if_fail(!from || !xfer);
	xf = xfer->data;
	g_return_if_fail(!xf);

	mrim = xf->mrim;

	if (xf->status == FT_STATUS_UNKNOWN)
	{
		// Отвечаем OK
		package *pack_ack = new_package(mrim->seq, MRIM_CS_FILE_TRANSFER_ACK);
		add_ul(FILE_TRANSFER_STATUS_OK, pack_ack);
		add_LPS(xf->from,pack_ack);
		add_ul(xf->session_id,pack_ack);
		add_LPS(NULL, pack_ack);
		send_package(pack_ack,mrim);

		// защита от повторного отправления сообщения
		xf->status = FT_STATUS_ACEPTED;

		// коннект
		//xf->ProxyConnectHandle = purple_proxy_connect(mrim->gc,mrim->account, ip, port, mrim_xfer_connect, xf);

	}


	purple_debug_info("mrim", "[%s] status=<%u>\n", __func__, xfer->status);


}


// INCOME

void mrim_process_file_transfer(mrim_data *mrim,package *pack)
{
	// TODO File Transfer
	gchar *from = read_LPS(pack);

	guint32 session_id = read_UL(pack);
	guint32 files_size = read_UL(pack);
	read_UL(pack); // skip хрень
	gchar *files_info = read_LPS(pack);
	gchar *more_file_info = read_UTF16LE(pack); // TODO FIX
	gchar *ips = read_LPS(pack);
	purple_debug_info("mrim","[%s] from=<%s>, total_file_size=<%u> ips=<%s>\n", __func__, from, files_size, ips);

	if(from && ips && files_info)
	{
		gchar **infos = g_strsplit(files_info, ";", 0);

		XepXfer *xf = g_new0(XepXfer, 1);
		xf->mrim = mrim;
		xf->from = from;
		xf->session_id = session_id;
		xf->total_files_size = files_size;
		xf->fd = 0;
		xf->status = FT_STATUS_UNKNOWN;

		int i=0;
		while (infos && infos[i] && infos[i+1])
		{
			gchar *fn = infos[i];
			int fs = atoi(infos[i+1]);
			i+=2;

			purple_debug_info("mrim","[%s] show FT request: <%s>:<%i>\n", __func__, fn, fs);

			PurpleXfer *xfer = purple_xfer_new(mrim->gc->account, PURPLE_XFER_RECEIVE, from);
			xfer->data = xf;
			xf->xfers = g_list_append(xf->xfers, xfer);


			purple_xfer_set_size(xfer,fs);
			purple_xfer_set_filename(xfer, fn);

			purple_xfer_set_init_fnc(xfer, mrim_xfer_init);
			purple_xfer_set_start_fnc( xfer, mrim_xfer_start );
			purple_xfer_set_cancel_send_fnc(xfer, mrim_xfer_cancel_send);
			purple_xfer_set_request_denied_fnc(xfer, mrim_xfer_denied);
			purple_xfer_set_end_fnc(xfer, mrim_xfer_end);
			purple_xfer_request(xfer);
		}
		if (i!=0)
			mrim->xfer_lists = g_list_append(mrim->xfer_lists, xf);
	}



	// FREE(from) // используется в xf
	FREE(files_info)
	FREE(more_file_info)
	FREE(ips)
}

void mrim_process_file_transfer_ack(mrim_data *mrim, package *pack)
{

}
