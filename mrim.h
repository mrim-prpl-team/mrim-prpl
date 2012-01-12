#ifndef MRIM_H
#define MRIM_H

#define PURPLE_PLUGINS

#include "config.h"

#define DISPLAY_VERSION "0.2.0"

#include <glib.h>

#define GETTEXT_PACKAGE "mrim-prpl-underbush"
#define LOCALEDIR "po"
#include <glib/gi18n-lib.h>

#include <sys/socket.h>
//#include <netinet/in.h> 
//#include <arpa/inet.h>

// libpurple
#include "accountopt.h"
#include "cipher.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "dnsquery.h"
#include "dnssrv.h"
#include "network.h"
#include "proxy.h"
#include "request.h"
#include "util.h"
#include "version.h"

#ifdef ENABLE_GTK
	#include <gtk/gtk.h>
#endif

#include "proto.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

typedef struct _MrimData MrimData;

#include "statuses.h"

struct _MrimData {
	PurpleAccount *account;
	PurpleConnection *gc;
	gchar *user_name;
	gchar *password;
	gchar *user_agent;
	gchar *balancer_host;
	guint balancer_port;
	gchar *server_host;
	guint server_port;
	PurpleUtilFetchUrlData *fetch_url;
	PurpleProxyConnectData *proxy_connect;
	int fd;
	guint32 seq;
	gpointer inp_package;
	guint keepalive_timeout;
	gint error_count;
	GHashTable *groups;
	GHashTable *acks;
	GHashTable *transfers;
	gboolean use_gtk;
	gchar *nick;
	MrimStatus *status;
	gchar *microblog;
	guint mail_count;
};

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
	WAITING_FOR_FT_GET
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

#include "util.h"

#define MRIM_MAIL_RU "mrim.mail.ru"
#define MRIM_MAIL_RU_PORT 2042
//#define MRIM_MAIL_RU_PORT 443

#define MRIM_MAX_ERROR_COUNT 16
#define MRIM_PRPL_ID "prpl-ostin-mrim-experimental"

#ifdef ENABLE_FILES
	#define COM_SUPPORT (FEATURE_FLAG_BASE_SMILES | FEATURE_FLAG_WAKEUP | FEATURE_FLAG_FILE_TRANSFER)
#else
	#define COM_SUPPORT (FEATURE_FLAG_BASE_SMILES | FEATURE_FLAG_WAKEUP)
#endif

#include "package.h"
#include "mrim-util.h"

typedef struct _MrimAck MrimAck;
struct _MrimAck {
	guint32 seq;
	void (*func)(MrimData *, gpointer, MrimPackage *);
	gpointer data;
};

typedef struct {
	guint count;
	gchar *from;
	gchar *subject;
} MrimNotifyMailData;

#define MRIM_MAGENT_ID "magent"

static struct {
	gchar *id;
	gchar *title;
} ua_titles[] = {
	{MRIM_MAGENT_ID, N_("Mail.ru Agent for Windows")},
	{"jagent", N_("Mail.ru Agent for Java")},
	{"android", N_("Mail.ru Agent for Android")},
	{"webagent", N_("Web-Agent@Mail.ru")},
	{"jmp", N_("Web-Agent@Mail.ru")},
	{"sagent", N_("Mail.ru Agent for Symbian")},
	{"iphoneagent", N_("Mail.ru Agent for iPhone")},
	{"QIP 2010", N_("QIP 2010")},
	{"QIP Infium", N_("QIP Infium")},
	{"mrim-prpl", N_("mrim-prpl")},
	{"mrimprpl", N_("libpurple plugin by lemax1@mail.ru")},
	{"ru.ibb.im.impl.mra.MraAccount", N_("QIP Mobile for Android")}
};

void mrim_add_ack_cb(MrimData *mrim, guint32 seq, void (*func)(MrimData *, gpointer, MrimPackage *), gpointer data);
void mrim_open_myworld_url(MrimData *mrim, gchar *user_name, gchar *fmt);
void mrim_post_microblog_record(MrimData *mrim, gchar *message);

PurplePlugin *mrim_plugin;

gchar *mrim_user_agent;

#endif
