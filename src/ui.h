/* FastNote C/GTK4 Edition — UI callbacks */

#ifndef FASTNOTE_UI_H
#define FASTNOTE_UI_H

#include <gtk/gtk.h>
#include "app.h"

void on_open_clicked(GtkWidget *widget, gpointer user_data);
void on_save_clicked(GtkWidget *widget, gpointer user_data);
void on_export_clicked(GtkWidget *widget, gpointer user_data);
void on_quit_clicked(GtkWidget *widget, gpointer user_data);

#endif /* FASTNOTE_UI_H */
