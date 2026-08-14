/* FastNote C/GTK4 Edition — Export module */

#ifndef FASTNOTE_EXPORT_H
#define FASTNOTE_EXPORT_H

#include <glib.h>

typedef struct _Exporter Exporter;

Exporter *exporter_new(void);
void exporter_free(Exporter *e);
gboolean exporter_write_html(Exporter *e, const gchar *filename, const gchar *content);
gboolean exporter_write_pdf(Exporter *e, const gchar *filename, const gchar *html_content);

#endif /* FASTNOTE_EXPORT_H */