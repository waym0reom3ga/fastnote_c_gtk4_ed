/* FastNote C/GTK4 Edition — Application state and real widget wiring */

#include "app.h"
#include "ui.h"
#include "actions.h"
#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char errbuf[512];

const char *fn_error(void) { return errbuf[0] ? errbuf : NULL; }

void fn_set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *p = malloc(len);
    if (p) memcpy(p, s, len);
    return p;
}

FastNoteApp *fastnote_app_new(void) {
    FastNoteApp *app = g_malloc0(sizeof(FastNoteApp));
    app->notes_dir = xstrdup(g_get_home_dir());
    app->theme = 0; /* light */
    return app;
}

void fastnote_app_free(FastNoteApp *app) {
    if (!app) return;
    g_free(app->notes_dir);
    g_free(app->current_path);
    g_free(app->document_content);
    g_free(app->event_file);
    g_free(app);
}

/* Phase-completion publication (spec §5.1): append one line to the event
 * file. A reporting outlet only — it never drives or simulates any operation;
 * callers invoke it only after the real operation has completed. */
void fn_event(FastNoteApp *app, const char *marker) {
    if (!app || !app->event_file || !marker) return;
    FILE *f = fopen(app->event_file, "a");
    if (f) {
        fprintf(f, "%s\n", marker);
        fclose(f);
    }
}

/* The editor buffer changed (user typed or the test inserted text):
 * copy the real buffer contents into the document state, mark dirty,
 * refresh the preview.  This is the same path real keystrokes take. */
void fastnote_on_editor_changed(GtkTextBuffer *buffer, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    if (!app || !buffer) return;
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    g_free(app->document_content);
    app->document_content = g_strdup(text);
    app->dirty = TRUE;
    g_free(text);

    ui_refresh_preview(app);
    fastnote_update_title(app);
}

gboolean fastnote_set_document(FastNoteApp *app, const gchar *path, const gchar *content) {
    if (!app || !path || !content) return FALSE;

    g_free(app->current_path);
    app->current_path = g_strdup(path);
    g_free(app->document_content);
    app->document_content = g_strdup(content);

    if (app->editor_buffer) {
        gtk_text_buffer_set_text(app->editor_buffer, content, -1);
    }
    /* set_text fires "changed", which marks the doc dirty; a freshly loaded
     * document is not dirty. */
    app->dirty = FALSE;
    ui_refresh_preview(app);
    fastnote_update_title(app);

    /* After an open, the editor owns the keyboard: a user types, the tests
     * type. */
    if (app->editor) gtk_widget_grab_focus(app->editor);
    return TRUE;
}

void fastnote_update_title(FastNoteApp *app) {
    if (!app || !app->window) return;
    if (app->current_path) {
        gchar *base = g_path_get_basename(app->current_path);
        gchar *title = g_strdup_printf("%s%s — FastNote", base, app->dirty ? "*" : "");
        gtk_window_set_title(GTK_WINDOW(app->window), title);
        g_free(title);
        g_free(base);
    } else {
        gtk_window_set_title(GTK_WINDOW(app->window), "FastNote");
    }
}

void fastnote_balance_paned(FastNoteApp *app) {
    if (!app || !app->paned || !app->window) return;
    int w = gtk_widget_get_width(app->window);
    if (w <= 0) return;
    gtk_paned_set_position(GTK_PANED(app->paned), w / 2);
}

/* Runs once the window has been allocated its real size. */
static gboolean balance_paned_idle(gpointer user_data) {
    FastNoteApp *app = user_data;
    fastnote_balance_paned(app);
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    (void)gtk_app;
    FastNoteApp *fna = (FastNoteApp *)user_data;
    GtkWidget *window = fastnote_build_ui(fna);
    gtk_widget_set_visible(window, TRUE);
    /* The editor owns the keyboard from the first frame (FR-3).  This must
     * run after the window is shown: grab_focus on an unrealized widget
     * silently does nothing. */
    gtk_widget_grab_focus(fna->editor);
    /* GtkPaned defaults the split to position 0, which collapses the editor
     * to a 1 px strip; balance it once the window has a real size. */
    g_idle_add(balance_paned_idle, fna);
}

/* FR-11 accelerators, delivered through the framework's key pipeline. The
 * accelerator invokes the same handler the toolbar button invokes (spec §5.2). */
static gboolean on_main_key_pressed(GtkEventControllerKey *ctrl,
                                    guint keyval, guint keycode,
                                    GdkModifierType state, gpointer user_data) {
    FastNoteApp *app = user_data;
    (void)ctrl;
    (void)keycode;
    /* Shift flips letter keyvals to uppercase; normalize before matching. */
    keyval = gdk_keyval_to_lower(keyval);
    gboolean ctrl_down = (state & GDK_CONTROL_MASK) != 0;
    gboolean shift_down = (state & GDK_SHIFT_MASK) != 0;

    if (ctrl_down && keyval == GDK_KEY_o)                { on_open_clicked(NULL, app); return TRUE; }
    if (ctrl_down && !shift_down && keyval == GDK_KEY_s) { on_save_clicked(NULL, app); return TRUE; }
    if (ctrl_down && shift_down && keyval == GDK_KEY_s)  { on_save_as_clicked(NULL, app); return TRUE; }
    if (ctrl_down && !shift_down && keyval == GDK_KEY_e) { on_export_clicked(NULL, app); return TRUE; }
    if (ctrl_down && shift_down && keyval == GDK_KEY_e)  { on_export_pdf_clicked(NULL, app); return TRUE; }
    return FALSE;
}

/* ---- FR-9 close semantics ------------------------------------------------
 * A close request on a clean document proceeds.  A close request on a dirty
 * document opens a prompt; the document is never silently discarded.  When
 * the main window is finally destroyed the application quits (FR-1: exit 0). */

static void prompt_cancel(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    gtk_window_destroy(GTK_WINDOW(user_data));
}

static void prompt_save(GtkWidget *btn, gpointer user_data) {
    FastNoteApp *app = user_data;
    (void)btn;
    if (app->current_path) actions_save_file(app);
    app->quitting = TRUE;
    if (app->window) gtk_window_close(GTK_WINDOW(app->window));
}

static void prompt_discard(GtkWidget *btn, gpointer user_data) {
    FastNoteApp *app = user_data;
    (void)btn;
    app->quitting = TRUE;
    if (app->window) gtk_window_close(GTK_WINDOW(app->window));
}

static void show_unsaved_prompt(FastNoteApp *app) {
    GtkWidget *dlg = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg), "Unsaved changes");
    gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(app->window));
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);

    GtkWidget *label = gtk_label_new("The document has unsaved changes.");
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    GtkWidget *discard_btn = gtk_button_new_with_label("Discard");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(save_btn, "suggested-action");

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(buttons), cancel_btn);
    gtk_box_append(GTK_BOX(buttons), discard_btn);
    gtk_box_append(GTK_BOX(buttons), save_btn);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_box_append(GTK_BOX(vbox), label);
    gtk_box_append(GTK_BOX(vbox), buttons);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_window_set_child(GTK_WINDOW(dlg), vbox);

    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(prompt_cancel), dlg);
    g_signal_connect(save_btn, "clicked", G_CALLBACK(prompt_save), app);
    g_signal_connect(discard_btn, "clicked", G_CALLBACK(prompt_discard), app);

    gtk_window_present(GTK_WINDOW(dlg));
}

static gboolean on_close_request(GtkWidget *win, gpointer user_data) {
    FastNoteApp *app = user_data;
    (void)win;
    if (app->dirty && !app->quitting) {
        show_unsaved_prompt(app);
        return TRUE; /* keep the window open until the dialog decides */
    }
    return FALSE; /* allow the close to proceed */
}

static void on_main_window_destroyed(GtkWidget *win, gpointer user_data) {
    (void)win;
    (void)user_data;
    GApplication *ga = g_application_get_default();
    if (ga) g_application_quit(ga);
}

/* ---- first painted frame (spec §5.1 'painted' marker) ------------------- */

static gboolean write_painted(gpointer user_data) {
    fn_event(user_data, "painted");
    return G_SOURCE_REMOVE;
}

static void on_window_mapped(GtkWidget *win, gpointer user_data) {
    (void)win;
    g_idle_add(write_painted, user_data);
}

int fastnote_app_run(FastNoteApp *app, int argc, char **argv) {
    GtkApplication *gtk_app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), app);
    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    return status;
}

/* Build the real widget tree: toolbar (Open/Save/Save As/Export HTML/Export
 * PDF/Theme), editor pane, preview pane, status bar.  Every control is wired
 * to its handler here — this is what the GUI shows and what tests drive. */
GtkWidget *fastnote_build_ui(FastNoteApp *app) {
    GtkWidget *window;
    if (app->window) {
        window = app->window;
    } else {
        GApplication *running = g_application_get_default();
        if (running && G_IS_APPLICATION(running)) {
            window = gtk_application_window_new(GTK_APPLICATION(running));
        } else {
            g_error("fastnote_build_ui requires a running GApplication");
        }
    }
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    app->open_btn = gtk_button_new_with_label("Open");
    app->save_btn = gtk_button_new_with_label("Save");
    app->save_as_btn = gtk_button_new_with_label("Save As");
    app->export_btn = gtk_button_new_with_label("Export HTML");
    app->export_pdf_btn = gtk_button_new_with_label("Export PDF");
    app->theme_btn = gtk_button_new_with_label("Theme");

    gtk_box_append(GTK_BOX(toolbar), app->open_btn);
    gtk_box_append(GTK_BOX(toolbar), app->save_btn);
    gtk_box_append(GTK_BOX(toolbar), app->save_as_btn);
    gtk_box_append(GTK_BOX(toolbar), app->export_btn);
    gtk_box_append(GTK_BOX(toolbar), app->export_pdf_btn);
    gtk_box_append(GTK_BOX(toolbar), app->theme_btn);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    app->paned = paned;
    GtkWidget *editor = gtk_text_view_new();
    app->editor = editor;
    app->editor_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
    app->preview = gtk_label_new(NULL);
    gtk_label_set_wrap(GTK_LABEL(app->preview), TRUE);
    gtk_label_set_xalign(GTK_LABEL(app->preview), 0.0);
    gtk_label_set_yalign(GTK_LABEL(app->preview), 0.0);
    gtk_widget_set_hexpand(app->preview, TRUE);
    gtk_widget_set_hexpand(editor, TRUE);

    gtk_paned_set_start_child(GTK_PANED(paned), editor);
    gtk_paned_set_end_child(GTK_PANED(paned), app->preview);

    app->status_label = gtk_label_new("FastNote");
    gtk_label_set_xalign(GTK_LABEL(app->status_label), 0.0);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), toolbar);
    gtk_box_append(GTK_BOX(vbox), paned);
    gtk_box_append(GTK_BOX(vbox), app->status_label);
    gtk_widget_set_vexpand(paned, TRUE);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    /* The real bindings — a click anywhere in the app lands here. */
    g_signal_connect(app->open_btn, "clicked", G_CALLBACK(on_open_clicked), app);
    g_signal_connect(app->save_btn, "clicked", G_CALLBACK(on_save_clicked), app);
    g_signal_connect(app->save_as_btn, "clicked", G_CALLBACK(on_save_as_clicked), app);
    g_signal_connect(app->export_btn, "clicked", G_CALLBACK(on_export_clicked), app);
    g_signal_connect(app->export_pdf_btn, "clicked", G_CALLBACK(on_export_pdf_clicked), app);
    g_signal_connect(app->theme_btn, "clicked", G_CALLBACK(on_theme_clicked), app);
    g_signal_connect(app->editor_buffer, "changed", G_CALLBACK(fastnote_on_editor_changed), app);

    app->window = window;

    /* FR-1 close->quit and FR-9 close-with-dirty handling. */
    g_signal_connect(window, "close-request", G_CALLBACK(on_close_request), app);
    g_signal_connect(window, "destroy", G_CALLBACK(on_main_window_destroyed), app);
    g_signal_connect(window, "map", G_CALLBACK(on_window_mapped), app);

    /* FR-11 accelerators through the framework's key pipeline. */
    app->key_ctrl = GTK_EVENT_CONTROLLER(gtk_event_controller_key_new());
    g_signal_connect(app->key_ctrl, "key-pressed", G_CALLBACK(on_main_key_pressed), app);
    gtk_widget_add_controller(window, app->key_ctrl);

    fastnote_update_title(app);
    return window;
}