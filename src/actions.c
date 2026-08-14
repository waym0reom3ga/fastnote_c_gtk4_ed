/* FastNote C/GTK4 Edition — Actions module */

#include "actions.h"
#include "app.h"
#include "renderer.h"
#include "export.h"
#include <stdio.h>
#include <string.h>

gboolean actions_save_file(FastNoteApp *app) {
    if (!app || !app->current_path || !app->document_content) return FALSE;
    
    FILE *f = fopen(app->current_path, "w");
    if (!f) {
        fn_set_error("Cannot save file: %s", app->current_path);
        return FALSE;
    }
    
    fwrite(app->document_content, strlen(app->document_content), 1, f);
    fclose(f);
    
    app->dirty = FALSE;
    return TRUE;
}

gboolean actions_save_as_file(FastNoteApp *app, const gchar *new_path) {
    if (!app || !new_path || !app->document_content) return FALSE;
    
    FILE *f = fopen(new_path, "w");
    if (!f) {
        fn_set_error("Cannot save file: %s", new_path);
        return FALSE;
    }
    
    fwrite(app->document_content, strlen(app->document_content), 1, f);
    fclose(f);
    
    g_free(app->current_path);
    app->current_path = g_strdup(new_path);
    app->dirty = FALSE;
    
    return TRUE;
}

gboolean actions_export_html(FastNoteApp *app, const gchar *output_path) {
    if (!app || !app->document_content || !output_path) return FALSE;
    
    Renderer *r = renderer_new();
    if (!renderer_render_markdown(r, app->document_content)) {
        renderer_free(r);
        return FALSE;
    }
    
    Exporter *e = exporter_new();
    gboolean success = exporter_write_html(e, output_path, renderer_get_html(r));
    
    exporter_free(e);
    renderer_free(r);
    
    return success;
}

gboolean actions_export_pdf(FastNoteApp *app, const gchar *output_path) {
    if (!app || !app->document_content || !output_path) return FALSE;
    
    Renderer *r = renderer_new();
    if (!renderer_render_markdown(r, app->document_content)) {
        renderer_free(r);
        return FALSE;
    }
    
    Exporter *e = exporter_new();
    gboolean success = exporter_write_pdf(e, output_path, renderer_get_html(r));
    
    exporter_free(e);
    renderer_free(r);
    
    return success;
}
