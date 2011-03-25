#ifndef MRIM_GTK__H_
#define MRIM_GTK__H_

// gtk+
#include <gtk/gtk.h>
#include "message.h"

typedef struct {
	PurpleBuddy *buddy;
	mrim_data *mrim;
	mrim_buddy *mb;
	GtkDialog *dialog;
	GtkTextView *message_text;
	GtkCheckButton *translit;
	GtkLabel *char_counter;
	GtkComboBox *phone;
	gchar *sms_text;
} sms_dialog_params;

void blist_sms_menu_item_gtk(PurpleBlistNode *node, gpointer userdata);
void update_sms_char_counter(GObject *object, sms_dialog_params *params);
void sms_dialog_response(GtkDialog *dialog, gint response_id, sms_dialog_params *params);
void sms_dialog_destroy(GtkDialog *dialog, sms_dialog_params *params);
void sms_dialog_edit_phones(GtkButton *button, sms_dialog_params *params);

#endif /* MRIM_GTK__H_ */
