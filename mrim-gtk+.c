#include "mrim.h"
#include "mrim-gtk+.h"

void blist_sms_menu_item_gtk(PurpleBlistNode *node, gpointer userdata) {
	PurpleBuddy *buddy = (PurpleBuddy *) node;
	mrim_data *mrim = userdata;
	g_return_if_fail(buddy != NULL);
	g_return_if_fail(mrim != NULL);
	mrim_buddy *mb = buddy->proto_data;
	g_return_if_fail(mb != NULL);

	/* Диалог */
	GtkDialog *dialog = gtk_dialog_new_with_buttons(_("Send SMS"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
	gtk_window_set_default_size(dialog, 320, 240);
	GtkWidget *content_area = gtk_dialog_get_content_area(dialog);
	GtkWidget *hbox;
	//gtk_container_set_border_width(content_area, 8); // Не понимаю почему не работает. Если когда-нибудь заработает - убрать следующую строчку
	gtk_container_set_border_width(dialog, 6);
	gtk_box_set_spacing(content_area, 6);
	/* Псевдоним */
	GtkLabel *buddy_name = gtk_label_new(mb->alias);
	gtk_box_pack_start(content_area, buddy_name, FALSE, TRUE, 0);
	/* Телефон */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(content_area, hbox, FALSE, TRUE, 0);
	GtkComboBox *phone_combo_box = gtk_combo_box_new_text();
	gtk_combo_box_append_text(phone_combo_box, mb->phones[0]);
	gtk_combo_box_append_text(phone_combo_box, mb->phones[1]);
	gtk_combo_box_append_text(phone_combo_box, mb->phones[2]);
	gtk_combo_box_set_active(phone_combo_box, 0);
	gtk_box_pack_start(hbox, gtk_label_new(_("Phone:")), FALSE, TRUE, 0);
	gtk_box_pack_start(hbox, phone_combo_box, TRUE, TRUE, 0);
	GtkButton *edit_phones_button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
	gtk_box_pack_end(hbox, edit_phones_button, FALSE, TRUE, 0);
	/* Текст сообщения */
	GtkScrolledWindow *scrolled_wnd = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(scrolled_wnd, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	GtkTextView *message_text = gtk_text_view_new();
	gtk_container_add(scrolled_wnd, message_text);
	gtk_box_pack_start(content_area, scrolled_wnd, TRUE, TRUE, 0);
	gtk_text_view_set_wrap_mode(message_text, GTK_WRAP_WORD);
	/* Флажок транслитерации и счётчик символов */
	hbox = gtk_hbutton_box_new();
	gtk_button_box_set_spacing(hbox, 6);
	gtk_button_box_set_layout(hbox, GTK_BUTTONBOX_EDGE);
	GtkCheckButton *translit = gtk_check_button_new_with_label(_("Translit"));
	gtk_container_add(hbox, translit);
	GtkLabel *char_counter = gtk_label_new("");
	gtk_container_add(hbox, char_counter);
	gtk_box_pack_end(content_area, hbox, FALSE, TRUE, 0);
	/* Сохраним адреса нужных объектов */
	sms_dialog_params *params = g_new0(sms_dialog_params, 1);
	params->buddy = buddy;
	params->mrim = mrim;
	params->mb = mb;
	params->message_text = message_text;
	params->translit = translit;
	params->char_counter = char_counter;
	params->phone = phone_combo_box;
	params->sms_text = NULL;
	/* Подключим обработчики сигналов */
	g_signal_connect(dialog, "destroy", sms_dialog_destroy, params);
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(message_text);
		g_signal_connect(buffer, "changed", update_sms_char_counter, params);
		update_sms_char_counter(buffer, params);
	}
	g_signal_connect(translit, "toggled", update_sms_char_counter, params);
	g_signal_connect(dialog, "response", sms_dialog_response, params);
	g_signal_connect(edit_phones_button, "clicked", sms_dialog_edit_phones, params);
	/* Пока выключим транслит */
	gtk_widget_set_sensitive(translit, FALSE);
	/* Отображаем диалог */
	gtk_widget_show_all(dialog);
	/* Делаем активным окном окно ввода сообщения */
	gtk_widget_grab_focus(message_text);
}


void update_sms_char_counter(GObject *object, sms_dialog_params *params) {
	gchar *original_text, *new_text;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(params->message_text);
	{
		GtkTextIter start, end;
		gtk_text_buffer_get_start_iter(buffer, &start);
		gtk_text_buffer_get_end_iter(buffer, &end);
		original_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	}
	if (gtk_toggle_button_get_active(params->translit)) {
		/* TODO: транслитерация сообщения */
		new_text = g_strdup(original_text); //new_text должен указывать на транслитерированный текст. original_text должен быть освобождён
	} else {
		new_text = g_strdup(original_text);
	}
	FREE(original_text);
	g_free(params->sms_text);
	params->sms_text = new_text;
	size_t count = g_utf8_strlen(new_text, -1);
	gchar buf[64];
	g_sprintf(&buf, _("Symbols: %d"), count);
	gtk_label_set_text(params->char_counter, buf);
}

void sms_dialog_response(GtkDialog *dialog, gint response_id, sms_dialog_params *params) {
	switch (response_id) {
		case GTK_RESPONSE_ACCEPT:
			{
				mrim_buddy *mb = params->mb;
				mrim_data *mrim = params->mrim;
				gchar *text = params->sms_text;
				gint phone_index = gtk_combo_box_get_active(params->phone);
				if (phone_index > -1) {
					gchar *phone = mb->phones[phone_index];
					mrim_send_sms(phone, text, mrim);
				}
				break;
			}
		case GTK_RESPONSE_REJECT:
			break;
	}
	gtk_widget_destroy(dialog);
}

void sms_dialog_destroy(GtkDialog *dialog, sms_dialog_params *params) {
	g_free(params->sms_text);
	g_free(params);
}

void sms_dialog_edit_phones(GtkButton *button, sms_dialog_params *params) {
	blist_edit_phones_menu_item(params->buddy, params->mrim);
	gtk_combo_box_remove_text(params->phone, 2);
	gtk_combo_box_remove_text(params->phone, 1);
	gtk_combo_box_remove_text(params->phone, 0);
	gtk_combo_box_append_text(params->phone, params->mb->phones[0]);
	gtk_combo_box_append_text(params->phone, params->mb->phones[1]);
	gtk_combo_box_append_text(params->phone, params->mb->phones[2]);
	gtk_combo_box_set_active(params->phone, 0);
}



