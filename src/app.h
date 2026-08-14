/* FastNote C/GTK4 Edition — Application state */

#ifndef FASTNOTE_APP_H
#define FASTNOTE_APP_H

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
    gchar *notes_dir;
    gchar *current_path;
    gchar *document_content;
    gboolean dirty;
    gint theme; /* 0 = light, 1 = dark */

    /* Widgets built by fastnote_build_ui — the real toolbar the user clicks */
    GtkWidget *window;
    GtkWidget *open_btn;
    GtkWidget *save_btn;
    GtkWidget *save_as_btn;
    GtkWidget *export_btn;
    GtkWidget *export_pdf_btn;
    GtkWidget *theme_btn;
    GtkTextBuffer *editor_buffer;
    GtkWidget *preview;
    GtkWidget *status_label;
} FastNoteApp;

FastNoteApp *fastnote_app_new(void);
void fastnote_app_free(FastNoteApp *app);
int fastnote_app_run(FastNoteApp *app, int argc, char **argv);
GtkWidget *fastnote_build_ui(FastNoteApp *app);

/* Load a document into the app: sets state, editor buffer, preview, title. */
gboolean fastnote_set_document(FastNoteApp *app, const gchar *path, const gchar *content);

/* Reflect the current document + dirty state in the window title. */
void fastnote_update_title(FastNoteApp *app);

const char *fn_error(void);
void fn_set_error(const char *fmt, ...);

#endif /* FASTNOTE_APP_H */