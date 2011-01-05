#ifndef FILETRANSFER_H_
 #define FILETRANSFER_H_
 #include "mrim.h"
 #include "ft.h" // this is in libpurple
 #include "package.h"


// не больше 128 мегабайт
#define MRIM_MAX_FILESIZE (1024*1024*128)

/**
 * Send a file.
 *
 * @param gc The PurpleConnection handle.
 * @param who Who are we sending it to?
 * @param file What file? If NULL, user will choose after this call.
 */
void mrim_send_file(PurpleConnection *gc, const char *who, const char *filename);

/**
 * Create a new PurpleXfer
 *
 * @param gc The PurpleConnection handle.
 * @param who Who will we be sending it to?
 */
PurpleXfer *mrim_xfer_new(PurpleConnection *gc, const char *who);


void mrim_process_file_transfer(mrim_data *mrim,package *pack);
void mrim_process_file_transfer_ack(mrim_data *mrim, package *pack);


typedef struct _XepXfer XepXfer;


#define FT_STATUS_UNKNOWN 0
#define FT_STATUS_ACEPTED 1
#define FT_STATUS_STARTED 2
#define FT_STATUS_DECLINE 4

struct _XepXfer
{
	mrim_data *mrim;
	guint32 session_id;
	gchar *from;
	guint32 total_files_size;
	GList *xfers;
	int status; // TODO

	int fd;
	PurpleProxyConnectData *ProxyConnectHandle;


	/*
	char *sid;
	char *recv_id;
	char *buddy_ip;
	int mode;
	PurpleNetworkListenData *listen_data;
	int sock5_req_state;
	int rxlen;
	char rx_buf[0x500];
	char tx_buf[0x500];
	PurpleProxyInfo *proxy_info;
	PurpleProxyConnectData *proxy_connection;
	char *jid;
	char *proxy_host;
	int proxy_port;
	*/
};



#endif /* FILETRANSFER_H_ */



