#ifndef MRIM_FT_H
#define MRIM_FT_H

#include "stdbool.h"
#include "util.h"
#include <glib.h>


#include "mrim.h"
#include "package.h"
#include "cl.h"

typedef struct _MrimFile MrimFile;

struct _MrimFile {
	// gchar *full_name;
	gchar *name;
	guint32 size;
};


enum MrimFTState
{	
	//mrim packages
	WAITING_FOR_HELLO_ACK,
	// plaintext
	WAITING_FOR_FT_HELLO,
	WAITING_FOR_FT_GET,
	STREAMING_FT_FILE
};

typedef struct _MrimFT MrimFT;

struct _MrimFT {
	MrimData *mrim;
	gchar *user_name; // please, document this field.
	guint32 id;
	guint32 proxy_id[4];
	gchar *remote_ip;
	MrimFile *files;
	guint count;
	guint current;
	PurpleProxyConnectData *proxy_conn;
	gint conn;
	void *inpa;
	PurpleXfer *xfer;
	int state;
	MrimData *fake_mrim;
};


gboolean mrim_can_send_file(PurpleConnection *gc, const char *who);
PurpleXfer *mrim_new_xfer(PurpleConnection *gc, const char *who);
void mrim_send_file(PurpleConnection *gc, const char *who, const char *file);
void mrim_xfer_cancel(PurpleXfer *xfer);

void mrim_xfer_send_rq(PurpleXfer *xfer);
void mrim_xfer_got_rq(MrimPackage *pack, MrimData *mrim);
void mrim_xfer_ack(MrimPackage *pack, MrimData *mrim);
void mrim_xfer_proxy_ack(MrimPackage *pack, MrimData *mrim);

void mrim_xfer_connected(MrimFT *ft);
void mrim_xfer_connect_cb(gpointer data, gint source, const gchar *error_message);
void mrim_process_xfer(MrimFT *ft);
void mrim_ft_send_input_cb(gpointer data, gint source, PurpleInputCondition cond);
void mrim_send_xfer_connect_cb(gpointer data, gint source, const gchar *error_message);

#endif
