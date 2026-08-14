/* FastNote C/GTK4 Edition — Markdown renderer */

#include "renderer.h"
#include <stdio.h>

struct _Renderer {
    gchar *html_output;
    GError *error;
};

/* Escape text so raw '<', '>', '&' cannot break the output HTML. */
static void append_escaped(GString *out, const gchar *start, gsize len) {
    for (gsize i = 0; i < len; i++) {
        switch (start[i]) {
            case '&':  g_string_append(out, "&amp;");  break;
            case '<':  g_string_append(out, "&lt;");   break;
            case '>':  g_string_append(out, "&gt;");   break;
            case '"':  g_string_append(out, "&quot;"); break;
            default:   g_string_append_c(out, start[i]);
        }
    }
}

gboolean renderer_render_markdown(Renderer *r, const gchar *markdown) {
    if (!r || !markdown) return FALSE;

    /* GString grows as needed: a heading-heavy document can expand a lot
     * ("# " -> "<h1></h1>\n"), so fixed-size buffers are unsafe. */
    GString *out = g_string_new(NULL);
    const gchar *in = markdown;
    const gchar *run = markdown; /* start of the unescaped text run */

    while (*in) {
        if (strncmp(in, "# ", 2) == 0) {
            if (in > run) append_escaped(out, run, (gsize)(in - run));
            in += 2;
            const gchar *end = strchr(in, '\n');
            gsize len = end ? (gsize)(end - in) : strlen(in);
            g_string_append(out, "<h1>");
            append_escaped(out, in, len);
            g_string_append(out, "</h1>\n");
            if (end) in = end + 1;
            else break;
            run = in;
        } else if (*in == '\n') {
            if (in > run) append_escaped(out, run, (gsize)(in - run));
            g_string_append_c(out, ' ');
            in++;
            run = in;
        } else {
            in++;
        }
    }
    if (in > run) append_escaped(out, run, (gsize)(in - run));

    g_free(r->html_output);
    r->html_output = g_string_free(out, FALSE);
    return TRUE;
}

const gchar *renderer_get_html(Renderer *r) {
    if (!r) return NULL;
    return r->html_output;
}

Renderer *renderer_new(void) {
    Renderer *r = g_malloc0(sizeof(Renderer));
    return r;
}

void renderer_free(Renderer *r) {
    if (!r) return;
    g_free(r->html_output);
    if (r->error) g_error_free(r->error);
    g_free(r);
}
