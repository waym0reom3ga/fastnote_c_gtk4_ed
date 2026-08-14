/* FastNote C/GTK4 Edition — Export module */

#include "export.h"
#include <stdio.h>
#include <string.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <pango/pangocairo.h>

struct _Exporter {
    gchar *html_content;
    GError *error;
};

gboolean exporter_write_html(Exporter *e, const gchar *filename, const gchar *content) {
    if (!e || !filename || !content) return FALSE;

    FILE *f = fopen(filename, "w");
    if (!f) {
        e->error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_ACCES,
                               "Cannot open file for writing: %s", filename);
        return FALSE;
    }

    fprintf(f,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <title>FastNote Export</title>\n"
        "  <style>body { font-family: sans-serif; max-width: 60em; margin: 2em auto; "
        "line-height: 1.5; } pre { background: #f4f4f4; padding: 1em; } "
        "code { background: #f4f4f4; padding: 0.1em 0.3em; }</style>\n"
        "</head>\n"
        "<body>\n%s\n</body>\n"
        "</html>\n", content);
    fclose(f);
    return TRUE;
}

/* Real PDF output via cairo's PDF surface: the rendered markdown (which is
 * already HTML) is reduced to text and drawn with Pango, so the PDF is a
 * genuine, viewable document — not a stub. */
gboolean exporter_write_pdf(Exporter *e, const gchar *filename, const gchar *html_content) {
    if (!e || !filename || !html_content) return FALSE;

    /* Strip tags for the PDF text: keep it simple and deterministic. */
    GString *text = g_string_new(NULL);
    const gchar *p = html_content;
    gboolean in_tag = FALSE;
    while (*p) {
        if (*p == '<') {
            in_tag = TRUE;
        } else if (*p == '>') {
            in_tag = FALSE;
        } else if (!in_tag) {
            /* Decode the entities the renderer emits, so the PDF shows the
             * real characters, not '&lt;'. */
            if (strncmp(p, "&lt;", 4) == 0)   { g_string_append_c(text, '<'); p += 3; }
            else if (strncmp(p, "&gt;", 4) == 0) { g_string_append_c(text, '>'); p += 3; }
            else if (strncmp(p, "&amp;", 5) == 0) { g_string_append_c(text, '&'); p += 4; }
            else if (strncmp(p, "&quot;", 6) == 0) { g_string_append_c(text, '"'); p += 5; }
            else g_string_append_c(text, *p);
        }
        p++;
    }

    cairo_surface_t *surface = cairo_pdf_surface_create(filename, 595.0, 842.0);
    cairo_status_t st = cairo_surface_status(surface);
    if (st != CAIRO_STATUS_SUCCESS) {
        e->error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED,
                               "Cannot create PDF %s: %s", filename,
                               cairo_status_to_string(st));
        cairo_surface_destroy(surface);
        g_string_free(text, TRUE);
        return FALSE;
    }

    cairo_t *cr = cairo_create(surface);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_from_string("Sans 11");
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, text->str, -1);
    pango_layout_set_width(layout, pango_units_from_double(555.0));
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, 20, 20);
    pango_cairo_show_layout(cr, layout);

    cairo_show_page(cr);

    g_object_unref(layout);
    pango_font_description_free(font);
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
    g_string_free(text, TRUE);
    return TRUE;
}

Exporter *exporter_new(void) {
    Exporter *e = g_malloc0(sizeof(Exporter));
    return e;
}

void exporter_free(Exporter *e) {
    if (!e) return;
    g_free(e->html_content);
    if (e->error) g_error_free(e->error);
    g_free(e);
}