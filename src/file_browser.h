/* FastNote C/GTK4 Edition — File browser component */

#ifndef FASTNOTE_FILE_BROWSER_H
#define FASTNOTE_FILE_BROWSER_H

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
    gchar *current_path;
    GList *file_list;
    GtkWidget *list_view;
    GtkWidget *path_entry;
    gboolean cancelled;
} FileBrowser;

FileBrowser *file_browser_new(void);
void file_browser_free(FileBrowser *fb);
void file_browser_set_start_dir(FileBrowser *fb, const char *dir);
const gchar *file_browser_get_selected_path(FileBrowser *fb);
gboolean file_browser_is_cancelled(FileBrowser *fb);
GtkWidget *file_browser_get_widget(FileBrowser *fb);

#endif /* FASTNOTE_FILE_BROWSER_H */
