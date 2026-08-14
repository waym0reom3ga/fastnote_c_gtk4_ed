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

    /* Phase-completion publication (spec §5.1): one line per completed
     * user-visible phase. NULL when not requested. */
    gchar *event_file;

    /* FR-9 close handling: set once the user confirmed discarding or saving
     * a dirty document, so the next close request may proceed. */
    gboolean quitting;

    /* Widgets built by fastnote_build_ui — the real toolbar the user clicks */
    GtkWidget *window;
    GtkWidget *open_btn;
    GtkWidget *save_btn;
    GtkWidget *save_as_btn;
    GtkWidget *export_btn;
    GtkWidget *export_pdf_btn;
    GtkWidget *theme_btn;
    GtkTextBuffer *editor_buffer;
    GtkWidget *editor;
    GtkWidget *preview;
    GtkWidget *status_label;
    GtkWidget *paned;

    /* Window-level key controller delivering the FR-11 accelerators. */
    GtkEventController *key_ctrl;
} FastNoteApp;

FastNoteApp *fastnote_app_new(void);
void fastnote_app_free(FastNoteApp *app);
int fastnote_app_run(FastNoteApp *app, int argc, char **argv);
GtkWidget *fastnote_build_ui(FastNoteApp *app);

/* Load a document into the app: sets state, editor buffer, preview, title. */
gboolean fastnote_set_document(FastNoteApp *app, const gchar *path, const gchar *content);

/* Reflect the current document + dirty state in the window title. */
void fastnote_update_title(FastNoteApp *app);

/* Keep the editor/preview paned split at half the window width.  GTK4's
 * GtkPaned defaults the separator to position 0, which collapses the start
 * child (the editor) to a 1 px strip; an explicit position restores it. */
void fastnote_balance_paned(FastNoteApp *app);

const char *fn_error(void);
void fn_set_error(const char *fmt, ...);

/* Append a phase marker to the event file, if one was requested (spec §5.1). */
void fn_event(FastNoteApp *app, const char *marker);

#endif /* FASTNOTE_APP_H */