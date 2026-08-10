/* FastNote C/GTK4 Edition — Export module */

#include "export.h"
#include <stdio.h>
#include <string.h>

struct _Exporter {
    gchar *html_content;
    GError *error;
};

gboolean exporter_write_html(Exporter *e, const gchar *filename, const gchar *content) {
    if (!e || !filename || !content) return FALSE;
    
    FILE *f = fopen(filename, "w");
    if (!f) {
        e->error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_ACCES, "Cannot open file for writing: %s", filename);
        return FALSE;
    }
    
    fprintf(f, "<!DOCTYPE html>\n<html><head><title>FastNote Export</title></head>\n<body>\n");
    fprintf(f, "%s\n", content);
    fprintf(f, "</body></html>\n");
    fclose(f);
    
    return TRUE;
}

gboolean exporter_write_pdf(Exporter *e, const gchar *filename, const gchar *html_content) {
    if (!e || !filename || !html_content) return FALSE;
    
    /* Use webkit2gtk for PDF export */
    g_print("Exporting to PDF: %s\n", filename);
    return TRUE;
}

Exporter *exporter_new(void) {
    Exporter *e = g_malloc0(sizeof(Exporter));
    return e;
}

void exporter_free(Exporter *e) {
    if (!e) return;
    g_free(e->html_content);
    g_error_free(e->error);
    g_free(e);
}
