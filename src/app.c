/* FastNote C/GTK4 Edition — Application state */

#include "app.h"
#include <gtk/gtk.h>
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char errbuf[512];

const char *fn_error(void) { return errbuf[0] ? errbuf : NULL; }

void fn_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

FastNoteApp *fastnote_app_new(void) {
    FastNoteApp *app = g_malloc0(sizeof(FastNoteApp));
    app->notes_dir = xstrdup(g_get_home_dir());
    app->theme = 0; /* light */
    return app;
}

void fastnote_app_free(FastNoteApp *app) {
    if (!app) return;
    g_free(app->notes_dir);
    g_free(app->current_path);
    g_free(app->document_content);
    g_free(app);
}

static void activate(GtkApplication *app, gpointer user_data) {
    FastNoteApp *fna = (FastNoteApp *)user_data;
    
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "FastNote");
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
    
    /* Create editor and preview panes */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *editor = gtk_text_view_new();
    GtkWidget *preview = gtk_label_new(NULL);
    
    gtk_paned_set_start_child(GTK_PANED(paned), editor);
    gtk_paned_set_end_child(GTK_PANED(paned), preview);
    
    /* Toolbar with Open/Save/Export buttons */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *open_btn = gtk_button_new_with_label("Open");
    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    GtkWidget *export_btn = gtk_button_new_with_label("Export");
    
    gtk_box_append(GTK_BOX(toolbar), open_btn);
    gtk_box_append(GTK_BOX(toolbar), save_btn);
    gtk_box_append(GTK_BOX(toolbar), export_btn);
    
    /* Pack everything */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), toolbar);
    gtk_box_append(GTK_BOX(vbox), paned);
    gtk_window_set_child(GTK_WINDOW(window), vbox);
    
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked), fna);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), fna);
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export_clicked), fna);
    
    gtk_widget_set_visible(window, TRUE);
}

int fastnote_app_run(FastNoteApp *app, int argc, char **argv) {
    /* Check for --version before GTK init */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("fastnote-c-gtk4 v1.0\n");
            return 0;
        }
    }
    
    /* Check for --headless before GTK init */
    int headless = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = 1;
            break;
        }
    }
    
    GtkApplication *gtk_app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), app);
    
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    
    g_object_unref(gtk_app);
    return status;
}
