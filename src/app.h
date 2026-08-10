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
} FastNoteApp;

FastNoteApp *fastnote_app_new(void);
void fastnote_app_free(FastNoteApp *app);
int fastnote_app_run(FastNoteApp *app, int argc, char **argv);

const char *fn_error(void);
void fn_set_error(const char *fmt, ...);

#endif /* FASTNOTE_APP_H */
