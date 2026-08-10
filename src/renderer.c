/* FastNote C/GTK4 Edition — Markdown renderer */

#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct _Renderer {
    gchar *html_output;
    GError *error;
};

static void escape_html(const gchar *input, gchar **output) {
    if (!input || !output) return;
    
    /* Simple HTML escaping */
    gsize len = strlen(input);
    *output = g_malloc(len * 6 + 1); /* Max expansion for & -> &amp; */
    
    gsize out_idx = 0;
    for (gsize i = 0; i < len; i++) {
        switch (input[i]) {
            case '&':
                strcpy(*output + out_idx, "&amp;");
                out_idx += 5;
                break;
            case '<':
                strcpy(*output + out_idx, "&lt;");
                out_idx += 4;
                break;
            case '>':
                strcpy(*output + out_idx, "&gt;");
                out_idx += 4;
                break;
            case '"':
                strcpy(*output + out_idx, "&quot;");
                out_idx += 6;
                break;
            default:
                (*output)[out_idx++] = input[i];
        }
    }
    (*output)[out_idx] = '\0';
}

gboolean renderer_render_markdown(Renderer *r, const gchar *markdown) {
    if (!r || !markdown) return FALSE;
    
    /* Simple markdown to HTML conversion */
    gsize len = strlen(markdown);
    r->html_output = g_malloc(len * 2 + 100); /* Buffer for HTML tags */
    
    gsize out_idx = 0;
    const gchar *in = markdown;
    
    while (*in) {
        if (strncmp(in, "# ", 2) == 0) {
            /* Heading */
            in += 2;
            const gchar *end = strchr(in, '\n');
            gsize heading_len = end ? (gsize)(end - in) : strlen(in);
            out_idx += sprintf(r->html_output + out_idx, "<h1>");
            for (gsize i = 0; i < heading_len && in[i] != '\n'; i++) {
                r->html_output[out_idx++] = in[i];
            }
            out_idx += sprintf(r->html_output + out_idx, "</h1>\n");
            if (end) in = end + 1;
            else break;
        } else if (*in == '\n') {
            r->html_output[out_idx++] = ' ';
            in++;
        } else {
            r->html_output[out_idx++] = *in++;
        }
    }
    
    r->html_output[out_idx] = '\0';
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
    g_error_free(r->error);
    g_free(r);
}
