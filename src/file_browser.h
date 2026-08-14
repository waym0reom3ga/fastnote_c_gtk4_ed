/* FastNote C/GTK4 Edition — File browser component */

#ifndef FASTNOTE_FILE_BROWSER_H
#define FASTNOTE_FILE_BROWSER_H

#include <glib.h>
#include <gtk/gtk.h>

typedef struct _FileBrowser FileBrowser;

/* A file browser built from GTK's own widgets: a path entry, a directory
 * list, and Up/Open/Cancel controls.  Selection is signalled to the caller
 * through a callback, exactly as a real click on a row would. */

FileBrowser *file_browser_new(const gchar *start_dir);
void file_browser_free(FileBrowser *fb);

/* Navigate the real widget state. */
gboolean file_browser_enter(FileBrowser *fb, const gchar *name);   /* open a subdir */
gboolean file_browser_parent(FileBrowser *fb);                     /* go up */
gboolean file_browser_set_dir(FileBrowser *fb, const gchar *dir);

const gchar *file_browser_current_dir(FileBrowser *fb);
const gchar *file_browser_selected_path(FileBrowser *fb);

/* The widget tree; must be packed into a window by the caller. */
GtkWidget *file_browser_get_widget(FileBrowser *fb);

/* The path entry (spec §3.2): a text field for typing a path directly.
 * Ctrl+L focuses it, Enter activates (directory -> navigate, file -> open). */
GtkWidget *file_browser_get_entry(FileBrowser *fb);

/* Callback typedef: invoked when the user activates a row (double-click /
 * Enter) or presses Open with a file selected.  cb is the user pointer. */
typedef void (*FileBrowserActivateFn)(FileBrowser *fb, const gchar *path,
                                      gpointer user_data);
void file_browser_set_activate_cb(FileBrowser *fb, FileBrowserActivateFn fn,
                                  gpointer user_data);

#endif /* FASTNOTE_FILE_BROWSER_H */