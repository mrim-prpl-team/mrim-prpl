
/*
{
	CURL *curl; //объект типа CURL
	curl = curl_easy_init(); //инициализация
	if(curl) //обязательная проверка
	{
		
	}
}

*/


/* Represents an active connection on an account. */
struct _PurpleConnection
{
	PurplePlugin *prpl;            /** The protocol plugin.              */
	PurpleConnectionFlags flags;   /** Connection flags.                 */
	PurpleConnectionState state;   /** The connection state.             */
	PurpleAccount *account;        /** The account being connected to.   */
	char *password;                /** The password used.                */
	int inpa;                      /** The input watcher.                */
	GSList *buddy_chats;           /** A list of active chats (#PurpleConversation structs of type  
	                                      #PURPLE_CONV_TYPE_CHAT).           */
	void *proto_data;              /** Protocol-specific data.           */
	char *display_name;            /** How you appear to other people.   */
	guint keepalive;               /** Keep-alive.                       */
	/** Wants to Die state.  This is set when the user chooses to log out, or
	 * when the protocol is disconnected and should not be automatically
	 * reconnected (incorrect password, etc.).  prpls should rely on
	 * purple_connection_error_reason() to set this for them rather than
	 * setting it themselves.
	 * @see purple_connection_error_is_fatal
	 */
	gboolean wants_to_die;
	guint disconnect_timeout;    /** Timer used for nasty stack tricks  */
	time_t last_received;        /** When we last received a packet. Set by the prpl to avoid sending 
	                                  unneeded keepalives */
};


/* Structure representing an account. */
struct _PurpleAccount
{
	char *username;             /**< The username.                          */
	char *alias;                /**< How you appear to yourself.            */
	char *password;             /**< The account password.                  */
	char *user_info;            /**< User information.                      */

	char *buddy_icon_path;      /**< The buddy icon's non-cached path.      */

	gboolean remember_pass;     /**< Remember the password.                 */

	char *protocol_id;          /**< The ID of the protocol.                */

	PurpleConnection *gc;         /**< The connection handle.                 */
	gboolean disconnecting;     /**< The account is currently disconnecting */

	GHashTable *settings;       /**< Protocol-specific settings.            */
	GHashTable *ui_settings;    /**< UI-specific settings.                  */

	PurpleProxyInfo *proxy_info;  /**< Proxy information.  This will be set   */
								/*   to NULL when the account inherits      */
								/*   proxy settings from global prefs.      */

	GSList *permit;             /**< Permit list.                           */
	GSList *deny;               /**< Deny list.                             */
	PurplePrivacyType perm_deny;  /**< The permit/deny setting.               */

	GList *status_types;        /**< Status types.                          */

	PurplePresence *presence;     /**< Presence.                              */
	PurpleLog *system_log;        /**< The system log                         */

	void *ui_data;              /**< The UI can put data here.              */
	PurpleAccountRegistrationCb registration_cb;
	void *registration_cb_user_data;

	gpointer priv;              /**< Pointer to opaque private data. */
};






typedef struct
{
	PurplePrefType type;      /**< The type of value.                     */
	char *text;               /**< The text that will appear to the user. */
	char *pref_name;          /**< The name of the associated preference. */

	union
	{
		gboolean boolean;   /**< The default boolean value.             */
		int integer;        /**< The default integer value.             */
		char *string;       /**< The default string value.              */
		GList *list;        /**< The default list value.                */

	} default_value;

	gboolean masked;        /**< Whether the value entered should be
	                         *   obscured from view (for passwords and
	                         *   similar options)
	                         */
} PurpleAccountOption;





/**
 * A Buddy list node.  This can represent a group, a buddy, or anything else.
 * This is a base class for PurpleBuddy, PurpleContact, PurpleGroup, and for
 * anything else that wants to put itself in the buddy list. */
struct _PurpleBlistNode {
	PurpleBlistNodeType type;             /**< The type of node this is       */
	PurpleBlistNode *prev;                /**< The sibling before this buddy. */
	PurpleBlistNode *next;                /**< The sibling after this buddy.  */
	PurpleBlistNode *parent;              /**< The parent of this node        */
	PurpleBlistNode *child;               /**< The child of this node         */
	GHashTable *settings;               /**< per-node settings              */
	void          *ui_data;             /**< The UI can put data here.      */
	PurpleBlistNodeFlags flags;           /**< The buddy flags                */
};

/**
 * A buddy.  This contains everything Purple will ever need to know about someone on the buddy list.  Everything.
 */
struct _PurpleBuddy {
	PurpleBlistNode node;                     /**< The node that this buddy inherits from */
	char *name;                             /**< The name of the buddy. */
	char *alias;                            /**< The user-set alias of the buddy */
	char *server_alias;                     /**< The server-specified alias of the buddy.  (i.e. MSN "Friendly Names") */
	void *proto_data;                       /**< This allows the prpl to associate whatever data it wants with a buddy */
	PurpleBuddyIcon *icon;                    /**< The buddy icon. */
	PurpleAccount *account;					/**< the account this buddy belongs to */
	PurplePresence *presence;
};

/**
 * A contact.  This contains everything Purple will ever need to know about a contact.
 */
struct _PurpleContact {
	PurpleBlistNode node;		/**< The node that this contact inherits from. */
	char *alias;            /**< The user-set alias of the contact */
	int totalsize;		    /**< The number of buddies in this contact */
	int currentsize;	    /**< The number of buddies in this contact corresponding to online accounts */
	int online;			    /**< The number of buddies in this contact who are currently online */
	PurpleBuddy *priority;    /**< The "top" buddy for this contact */
	gboolean priority_valid; /**< Is priority valid? */
};


/**
 * A group.  This contains everything Purple will ever need to know about a group.
 */
struct _PurpleGroup {
	PurpleBlistNode node;                    /**< The node that this group inherits from */
	char *name;                            /**< The name of this group. */
	int totalsize;			       /**< The number of chats and contacts in this group */
	int currentsize;		       /**< The number of chats and contacts in this group corresponding to online accounts */
	int online;			       /**< The number of chats and contacts in this group who are currently online */
};

/**
 * A chat.  This contains everything Purple needs to put a chat room in the
 * buddy list.
 */
struct _PurpleChat {
	PurpleBlistNode node;      /**< The node that this chat inherits from */
	char *alias;             /**< The display name of this chat. */
	GHashTable *components;  /**< the stuff the protocol needs to know to join the chat */
	PurpleAccount *account; /**< The account this chat is attached to */
};




#include <zlib.h>

/*   */
static gchar *vk_gunzip(const guchar *gzip_data, ssize_t *len_ptr)
{
    gsize gzip_data_len= *len_ptr;
    z_stream zstr;
    int gzip_err = 0;
    gchar *data_buffer;
    gulong gzip_len = G_MAXUINT16;
    GString *output_string = NULL;

    data_buffer = g_new0(gchar, gzip_len);

    zstr.next_in = NULL;
    zstr.avail_in = 0;
    zstr.zalloc = Z_NULL;
    zstr.zfree = Z_NULL;
    zstr.opaque = 0;
    gzip_err = inflateInit2(&zstr, MAX_WBITS+32);
    if (gzip_err != Z_OK)
    {
        g_free(data_buffer);
        purple_debug_error("vk", "no built-in gzip support in zlib\n");
        return NULL;
    }

    zstr.next_in = (Bytef *)gzip_data;
    zstr.avail_in = gzip_data_len;

    zstr.next_out = (Bytef *)data_buffer;
    zstr.avail_out = gzip_len;

    gzip_err = inflate(&zstr, Z_SYNC_FLUSH);

    if (gzip_err == Z_DATA_ERROR)
    {
        inflateEnd(&zstr);
        inflateInit2(&zstr, -MAX_WBITS);
        if (gzip_err != Z_OK)
        {
            g_free(data_buffer);
            purple_debug_error("vk", "Cannot decode gzip header\n");
            return NULL;
        }
        zstr.next_in = (Bytef *)gzip_data;
        zstr.avail_in = gzip_data_len;
        zstr.next_out = (Bytef *)data_buffer;
        zstr.avail_out = gzip_len;
        gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
    }

    output_string = g_string_new("");

    while (gzip_err == Z_OK)
    {
        //append data to buffer
        output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
        //reset buffer pointer
        zstr.next_out = (Bytef *)data_buffer;
        zstr.avail_out = gzip_len;
        gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
    }

    if (gzip_err == Z_STREAM_END)
    {
        output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
    }
    else
    {
        purple_debug_error("vk", "gzip inflate error\n");
    }
    inflateEnd(&zstr);

    g_free(data_buffer);

    if (len_ptr)
    {
        *len_ptr = output_string->len;
    }

    return g_string_free(output_string, FALSE);
}








/* moods (reference: libpurple/status.h)
static PurpleMood mrim_moods[] =
{	// x3               i18n            NULL
	{"angry",		N_("Angry"),		NULL},
	{"excited",		N_("Excited"),		NULL},
	{"grumpy",		N_("Grumpy"),		NULL},
	{"happy",		N_("Happy"),		NULL},
	{"in_love",		N_("In love"),		NULL},
	{"invincible",	N_("Invincible"),	NULL},
	{"sad",			N_("Sad"),			NULL},
	{"hot",			N_("Hot"),			NULL},
	{"sick",		N_("Sick"),			NULL},
	{"sleepy",		N_("Sleepy"),		NULL},
	// Mark the last record.
	{ NULL, NULL, NULL }
};
*/


/*

int mrim_convert_mood( const char* id )
{
	unsigned int	i;

	// Mood is being unset
	if ( id == NULL )
		return MRIM_MOOD_NONE;

	for ( i = 0; i < ARRAY_SIZE( mxit_moods ) - 1; i++ ) {
		if ( strcmp( mxit_moods[i].mood, id ) == 0 )	// mood found!
			return i + 1;		// because MXIT_MOOD_NONE is 0
	}

	return -1;
}


PurpleMood* mxit_get_moods(PurpleAccount *account)
{
	return mrim_moods;
}



const char* mxit_convert_mood_to_name( short id )
{
	switch ( id ) {
		case MXIT_MOOD_ANGRY :
				return _( "Angry" );
		case MXIT_MOOD_EXCITED :
				return _( "Excited" );
		case MXIT_MOOD_GRUMPY :
				return _( "Grumpy" );
		case MXIT_MOOD_HAPPY :
				return _( "Happy" );
		case MXIT_MOOD_INLOVE :
				return _( "In Love" );
		case MXIT_MOOD_INVINCIBLE :
				return _( "Invincible" );
		case MXIT_MOOD_SAD :
				return _( "Sad" );
		case MXIT_MOOD_HOT :
				return _( "Hot" );
		case MXIT_MOOD_SICK :
				return _( "Sick" );
		case MXIT_MOOD_SLEEPY :
				return _( "Sleepy" );
		case MXIT_MOOD_NONE :
		default :
				return "";
	}
}
*/



/*------------------------------------------------------------------------
 * A presence update packet was received from the MXit server, so update the buddy's
 * information.
 *
 *  @param session		The MXit session object
 *  @param username		The contact which presence to update
 *  @param presence		The new presence state for the contact
 *  @param mood			The new mood for the contact
 *  @param customMood	The custom mood identifier
 *  @param statusMsg	This is the contact's status message
 *  @param avatarId		This is the contact's avatar id
 */
void mxit_update_buddy_presence( struct MXitSession* session, const char* username, short presence, short mood, const char* customMood, const char* statusMsg, const char* avatarId )
{
	PurpleBuddy*		buddy	= NULL;
	struct contact*		contact	= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_update_buddy_presence: user='%s' presence=%i mood=%i customMood='%s' statusMsg='%s' avatar='%s'\n",
		username, presence, mood, customMood, statusMsg, avatarId );

	if ( ( presence < MXIT_PRESENCE_OFFLINE ) || ( presence > MXIT_PRESENCE_DND ) ) {
		purple_debug_info( MXIT_PLUGIN_ID, "mxit_update_buddy_presence: invalid presence state %i\n", presence );
		return;		/* ignore packet */
	}

	/* find the buddy information for this contact (reference: "libpurple/blist.h") */
	buddy = purple_find_buddy( session->acc, username );
	if ( !buddy ) {
		purple_debug_warning( MXIT_PLUGIN_ID, "mxit_update_buddy_presence: unable to find the buddy '%s'\n", username );
		return;
	}

	contact = purple_buddy_get_protocol_data( buddy );
	if ( !contact )
		return;

	contact->presence = presence;
	contact->mood = mood;

	/* validate mood */
	if (( contact->mood < MXIT_MOOD_NONE ) || ( contact->mood > MXIT_MOOD_SLEEPY ))
		contact->mood = MXIT_MOOD_NONE;

	g_strlcpy( contact->customMood, customMood, sizeof( contact->customMood ) );
	// TODO: Download custom mood frame.

	/* update status message */
	if ( contact->statusMsg ) {
		g_free( contact->statusMsg );
		contact->statusMsg = NULL;
	}
	if ( statusMsg[0] != '\0' )
		contact->statusMsg = g_markup_escape_text( statusMsg, -1 );

	/* update avatarId */
	if ( ( contact->avatarId ) && ( g_ascii_strcasecmp( contact->avatarId, avatarId ) == 0 ) ) {
		/*  avatar has not changed - do nothing */
	}
	else if ( avatarId[0] != '\0' ) {		/* avatar has changed */
		if ( contact->avatarId )
			g_free( contact->avatarId );
		contact->avatarId = g_strdup( avatarId );

		/* Send request to download new avatar image */
		mxit_get_avatar( session, username, avatarId );
	}
	else		/* clear current avatar */
		purple_buddy_icons_set_for_user( session->acc, username, NULL, 0, NULL );

	/* update the buddy's status (reference: "libpurple/prpl.h") */
	if ( contact->statusMsg )
		purple_prpl_got_user_status( session->acc, username, mxit_statuses[contact->presence].id, "message", contact->statusMsg, NULL );
	else
		purple_prpl_got_user_status( session->acc, username, mxit_statuses[contact->presence].id, NULL );

	/* update the buddy's mood */
	if ( contact->mood == MXIT_MOOD_NONE )
		purple_prpl_got_user_status_deactive( session->acc, username, "mood" );
	else
		purple_prpl_got_user_status( session->acc, username, "mood", PURPLE_MOOD_NAME, mxit_moods[contact->mood-1].mood, NULL );
}






#ifndef _WIN32
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/time.h>
#endif





#ifndef _WIN32
# include <netinet/in.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <sys/un.h>
# include <sys/utsname.h>
# include <netdb.h>
# include <signal.h>
# include <unistd.h>
#endif



#ifdef _WIN32
#include "win32dep.h"
#endif









static gchar *fb_gunzip(const guchar *gzip_data, ssize_t *len_ptr)
{
	gsize gzip_data_len	= *len_ptr;
	z_stream zstr;
	int gzip_err = 0;
	gchar *data_buffer;
	gulong gzip_len = G_MAXUINT16;
	GString *output_string = NULL;

	data_buffer = g_new0(gchar, gzip_len);

	zstr.next_in = NULL;
	zstr.avail_in = 0;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = 0;
	gzip_err = inflateInit2(&zstr, MAX_WBITS+32);
	if (gzip_err != Z_OK)
	{
		g_free(data_buffer);
		purple_debug_error("juice", "no built-in gzip support in zlib\n");
		return NULL;
	}

	zstr.next_in = (Bytef *)gzip_data;
	zstr.avail_in = gzip_data_len;

	zstr.next_out = (Bytef *)data_buffer;
	zstr.avail_out = gzip_len;

	gzip_err = inflate(&zstr, Z_SYNC_FLUSH);

	if (gzip_err == Z_DATA_ERROR)
	{
		inflateEnd(&zstr);
		inflateInit2(&zstr, -MAX_WBITS);
		if (gzip_err != Z_OK)
		{
			g_free(data_buffer);
			purple_debug_error("juice", "Cannot decode gzip header\n");
			return NULL;
		}
		zstr.next_in = (Bytef *)gzip_data;
		zstr.avail_in = gzip_data_len;
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	output_string = g_string_new("");
	while (gzip_err == Z_OK)
	{
		//append data to buffer
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
		//reset buffer pointer
		zstr.next_out = (Bytef *)data_buffer;
		zstr.avail_out = gzip_len;
		gzip_err = inflate(&zstr, Z_SYNC_FLUSH);
	}
	if (gzip_err == Z_STREAM_END)
	{
		output_string = g_string_append_len(output_string, data_buffer, gzip_len - zstr.avail_out);
	} else {
		purple_debug_error("juice", "gzip inflate error\n");
	}
	inflateEnd(&zstr);

	g_free(data_buffer);

	if (len_ptr)
		*len_ptr = output_string->len;

	return g_string_free(output_string, FALSE);
}
#endif





maksbotan: пропихни в санрайс
maksbotan: или пинай слепногу чтоб в рион взял
winterheart: кейворды не должны быть стабильными, только ~arch - требование любого оверлея
winterheart: src_compile можно вообще убрать
winterheart: пропиши переменную S="${WORKDIR}"/mrim-prpl под RDEPEND
winterheart: и src_install при таком раскладе тоже можно будет убрать
winterheart: ostinru: ну и repoman пройдись
winterheart: PF не используют в общем случае. лучше  ${P}
winterheart: у тебя же не используется ревизия в названии тарбола
ostinru: ок. (у меня раньше использовалось - означало ревизию в svn, но потом я забил на это)
winterheart: тэкс
winterheart: как проверишь, скинь куда у тебя файлы скидываются
winterheart: у тебя в Makefile ошибка
winterheart: install -Dm0644 mrim.so ${DESTDIR}/usr/lib/purple-2/mrim.so
winterheart: 25 строка
winterheart: для 64битных систем не пойдет так
winterheart: у них lib64 должен быть
winterheart: сделай переменные для DATADIR и LIBDIR и пропихивай правильные значения в src_install
winterheart: да, еще в Makefile 21 строка лишняя
