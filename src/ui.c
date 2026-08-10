/* FastNote C/GTK4 Edition — UI callbacks */

#include "ui.h"
#include "actions.h"
#include "file_browser.h"
#include <gtk/gtk.h>

static void open_file_dialog(FastNoteApp *app) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Open File");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    
    FileBrowser *fb = file_browser_new();
    file_browser_set_start_dir(fb, app->notes_dir);
    
    GtkWidget *list_view = file_browser_get_widget(fb);
    gtk_window_set_child(GTK_WINDOW(dialog), list_view);
    
    g_signal_connect(dialog, "destroy", G_CALLBACK(gtk_window_destroy), NULL);
    
    gtk_widget_set_visible(dialog, TRUE);
}

void on_open_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    if (app) open_file_dialog(app);
    (void)widget;
}

void on_save_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    if (app && app->document_content) {
        actions_save_file(app);
    }
    (void)widget;
}

void on_export_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    if (app && app->document_content) {
        gchar *output_path = g_build_filename(g_get_home_dir(), "export.html", NULL);
        actions_export_html(app, output_path);
        g_free(output_path);
    }
    (void)widget;
}

void on_quit_clicked(GtkWidget *widget, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(widget));
    (void)user_data;
}
