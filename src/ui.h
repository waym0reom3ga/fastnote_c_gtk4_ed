/* FastNote C/GTK4 Edition — UI callbacks */

#ifndef FASTNOTE_UI_H
#define FASTNOTE_UI_H

#include <gtk/gtk.h>
#include "app.h"

void on_open_clicked(GtkWidget *widget, gpointer user_data);
void on_save_clicked(GtkWidget *widget, gpointer user_data);
void on_save_as_clicked(GtkWidget *widget, gpointer user_data);
void on_save_as_confirm(GtkWidget *widget, gpointer user_data);
void on_export_clicked(GtkWidget *widget, gpointer user_data);
void on_export_pdf_clicked(GtkWidget *widget, gpointer user_data);
void on_export_confirm(GtkWidget *widget, gpointer user_data);
void on_theme_clicked(GtkWidget *widget, gpointer user_data);
void on_quit_clicked(GtkWidget *widget, gpointer user_data);

void ui_refresh_preview(FastNoteApp *app);
GtkWidget *ui_active_dialog(void);

#endif /* FASTNOTE_UI_H */