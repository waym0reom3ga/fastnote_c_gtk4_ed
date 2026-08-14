/* FastNote C/GTK4 Edition — Application state and real widget wiring */

#include "app.h"
#include "ui.h"
#include "actions.h"
#include "renderer.h"
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

/* The editor buffer changed (user typed or the test inserted text):
 * copy the real buffer contents into the document state, mark dirty,
 * refresh the preview.  This is the same path real keystrokes take. */
void fastnote_on_editor_changed(GtkTextBuffer *buffer, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    if (!app || !buffer) return;
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    g_free(app->document_content);
    app->document_content = g_strdup(text);
    app->dirty = TRUE;
    g_free(text);

    ui_refresh_preview(app);
    fastnote_update_title(app);
}

gboolean fastnote_set_document(FastNoteApp *app, const gchar *path, const gchar *content) {
    if (!app || !path || !content) return FALSE;

    g_free(app->current_path);
    app->current_path = g_strdup(path);
    g_free(app->document_content);
    app->document_content = g_strdup(content);

    if (app->editor_buffer) {
        gtk_text_buffer_set_text(app->editor_buffer, content, -1);
    }
    /* set_text fires "changed", which marks the doc dirty; a freshly loaded
     * document is not dirty. */
    app->dirty = FALSE;
    ui_refresh_preview(app);
    fastnote_update_title(app);
    return TRUE;
}

void fastnote_update_title(FastNoteApp *app) {
    if (!app || !app->window) return;
    if (app->current_path) {
        gchar *base = g_path_get_basename(app->current_path);
        gchar *title = g_strdup_printf("%s%s — FastNote", base, app->dirty ? "*" : "");
        gtk_window_set_title(GTK_WINDOW(app->window), title);
        g_free(title);
        g_free(base);
    } else {
        gtk_window_set_title(GTK_WINDOW(app->window), "FastNote");
    }
}

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)gtk_app;
    FastNoteApp *fna = (FastNoteApp *)user_data;
    GtkWidget *window = fastnote_build_ui(fna);
    gtk_widget_set_visible(window, TRUE);
}

int fastnote_app_run(FastNoteApp *app, int argc, char **argv) {
    GtkApplication *gtk_app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), app);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}

/* Build the real widget tree: toolbar (Open/Save/Save As/Export HTML/Export
 * PDF/Theme), editor pane, preview pane, status bar.  Every control is wired
 * to its handler here — this is what the GUI shows and what tests drive. */
GtkWidget *fastnote_build_ui(FastNoteApp *app) {
    GtkWidget *window;
    if (app->window) {
        window = app->window;
    } else {
        GApplication *running = g_application_get_default();
        if (running && G_IS_APPLICATION(running)) {
            window = gtk_application_window_new(GTK_APPLICATION(running));
        } else {
            g_error("fastnote_build_ui requires a running GApplication");
        }
    }
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    app->open_btn = gtk_button_new_with_label("Open");
    app->save_btn = gtk_button_new_with_label("Save");
    app->save_as_btn = gtk_button_new_with_label("Save As");
    app->export_btn = gtk_button_new_with_label("Export HTML");
    app->export_pdf_btn = gtk_button_new_with_label("Export PDF");
    app->theme_btn = gtk_button_new_with_label("Theme");

    gtk_box_append(GTK_BOX(toolbar), app->open_btn);
    gtk_box_append(GTK_BOX(toolbar), app->save_btn);
    gtk_box_append(GTK_BOX(toolbar), app->save_as_btn);
    gtk_box_append(GTK_BOX(toolbar), app->export_btn);
    gtk_box_append(GTK_BOX(toolbar), app->export_pdf_btn);
    gtk_box_append(GTK_BOX(toolbar), app->theme_btn);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *editor = gtk_text_view_new();
    app->editor_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
    app->preview = gtk_label_new(NULL);
    gtk_label_set_wrap(GTK_LABEL(app->preview), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app->preview), 0.0);
    gtk_label_set_yalign(GTK_LABEL(app->preview), 0.0);
    gtk_widget_set_hexpand(app->preview, TRUE);
    gtk_widget_set_hexpand(editor, TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), editor);
    gtk_paned_set_end_child(GTK_PANED(paned), app->preview);

    app->status_label = gtk_label_new("FastNote");
    gtk_label_set_xalign(GTK_LABEL(app->status_label), 0.0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), toolbar);
    gtk_box_append(GTK_BOX(vbox), paned);
    gtk_box_append(GTK_BOX(vbox), app->status_label);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    /* The real bindings — a click anywhere in the app lands here. */
    g_signal_connect(app->open_btn, "clicked", G_CALLBACK(on_open_clicked), app);
    g_signal_connect(app->save_btn, "clicked", G_CALLBACK(on_save_clicked), app);
    g_signal_connect(app->save_as_btn, "clicked", G_CALLBACK(on_save_as_clicked), app);
    g_signal_connect(app->export_btn, "clicked", G_CALLBACK(on_export_clicked), app);
    g_signal_connect(app->export_pdf_btn, "clicked", G_CALLBACK(on_export_pdf_clicked), app);
    g_signal_connect(app->theme_btn, "clicked", G_CALLBACK(on_theme_clicked), app);
    g_signal_connect(app->editor_buffer, "changed", G_CALLBACK(fastnote_on_editor_changed), app);

    app->window = window;
    fastnote_update_title(app);
    return window;
}