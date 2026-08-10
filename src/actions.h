/* FastNote C/GTK4 Edition — Actions module */

#ifndef FASTNOTE_ACTIONS_H
#define FASTNOTE_ACTIONS_H

#include <glib.h>
#include "app.h"

gboolean actions_open_file(FastNoteApp *app, const gchar *filename);
gboolean actions_save_file(FastNoteApp *app);
gboolean actions_save_as_file(FastNoteApp *app, const gchar *new_path);
gboolean actions_export_html(FastNoteApp *app, const gchar *output_path);
gboolean actions_export_pdf(FastNoteApp *app, const gchar *output_path);

#endif /* FASTNOTE_ACTIONS_H */
