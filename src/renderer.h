/* FastNote C/GTK4 Edition — Markdown renderer */

#ifndef FASTNOTE_RENDERER_H
#define FASTNOTE_RENDERER_H

#include <glib.h>

typedef struct {
    gchar *html_output;
    GError *error;
} Renderer;

Renderer *renderer_new(void);
void renderer_free(Renderer *r);
gboolean renderer_render_markdown(Renderer *r, const gchar *markdown);
const gchar *renderer_get_html(Renderer *r);

#endif /* FASTNOTE_RENDERER_H */
