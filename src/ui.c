/* FastNote C/GTK4 Edition — UI callbacks */

#include "ui.h"
#include "actions.h"
#include "file_browser.h"
#include "renderer.h"
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <string.h>

static void on_browser_activate(FileBrowser *fb, const gchar *path, gpointer user_data);

/* The active modal dialog (open browser or export save dialog). */
static GtkWidget *active_dialog = NULL;
static FastNoteApp *dialog_app = NULL;

static void dialog_close(void);

/* The file browser keyboard contract (spec §3.2): Ctrl+L focuses the path
 * entry and selects it; Escape cancels.  Enter activates through the entry's
 * own "activate" signal and through the framework's default handling. */
static gboolean on_dialog_key_pressed(GtkEventControllerKey *ctrl, guint keyval,
                                      guint keycode, GdkModifierType state,
                                      gpointer user_data) {
    GtkWidget *entry = user_data;
    (void)ctrl;
    (void)keycode;
    if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_l) {
        gtk_widget_grab_focus(entry);
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
        return TRUE;
    }
    if (keyval == GDK_KEY_Escape) {
        dialog_close();
        return TRUE;
    }
    return FALSE;
}

static void add_dialog_keys(GtkWidget *win, GtkWidget *entry) {
    GtkEventController *kc = gtk_event_controller_key_new();
    g_signal_connect(kc, "key-pressed", G_CALLBACK(on_dialog_key_pressed), entry);
    gtk_widget_add_controller(win, kc);
}

struct ExportDialogCtx {
    FastNoteApp *app;
    gboolean pdf;
};

/* Render the current document into the preview label.  The preview shows the
 * HTML the renderer produced — visually distinct from the raw editor text. */
void ui_refresh_preview(FastNoteApp *app) {
    if (!app || !app->preview) return;
    if (!app->document_content) {
        gtk_label_set_text(GTK_LABEL(app->preview), "");
        return;
    }
    Renderer *r = renderer_new();
    if (!renderer_render_markdown(r, app->document_content)) {
        renderer_free(r);
        gtk_label_set_text(GTK_LABEL(app->preview), "");
        return;
    }
    gtk_label_set_text(GTK_LABEL(app->preview), renderer_get_html(r));
    renderer_free(r);
}

/* A dialog just lost keyboard focus.  Restore it to the editor so typing
 * reaches the document (FR-3).  Deferred to the next idle iteration: the
 * destroy must finish and the main window must be re-established as the
 * focused toplevel before grab_focus can succeed.  The paned split is also
 * rebalanced — the dialog cycle can reset GtkPaned's position to 0, which
 * collapses the editor to a 1 px strip. */
static gboolean refocus_editor(gpointer user_data) {
    FastNoteApp *app = user_data;
    if (app && app->window) {
        if (app->editor) {
            if (!gtk_widget_get_mapped(app->editor)) {
                gtk_widget_set_visible(app->editor, FALSE);
                gtk_widget_set_visible(app->editor, TRUE);
            }
            gtk_widget_grab_focus(app->editor);
        }
        fastnote_balance_paned(app);
    }
    return G_SOURCE_REMOVE;
}

static void dialog_close(void) {
    if (active_dialog) {
        FastNoteApp *app = dialog_app;
        gtk_window_destroy(GTK_WINDOW(active_dialog));
        active_dialog = NULL;
        dialog_app = NULL;
        if (app) g_idle_add(refocus_editor, app);
    }
}

/* Exposed for the GUI test suite: the currently open modal dialog. */
GtkWidget *ui_active_dialog(void) { return active_dialog; }

/* Open file browser: browse any directory, select a file, load it. */
static void open_file_dialog(FastNoteApp *app) {
    if (active_dialog) dialog_close();

    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Open File");
    gtk_window_set_default_size(GTK_WINDOW(win), 640, 420);
    if (app->window) {
        gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(app->window));
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    }

    FileBrowser *fb = file_browser_new(app->notes_dir);
    file_browser_set_activate_cb(fb, on_browser_activate, app);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(dialog_close), NULL);

    GtkWidget *browser = file_browser_get_widget(fb);
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(cancel_btn, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(bottom), cancel_btn);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(vbox), browser);
    gtk_box_append(GTK_BOX(vbox), bottom);
    gtk_window_set_child(GTK_WINDOW(win), vbox);
    gtk_widget_set_vexpand(browser, TRUE);

    add_dialog_keys(win, file_browser_get_entry(fb));

    active_dialog = win;
    dialog_app = app;
    gtk_window_present(GTK_WINDOW(win));
}

/* A row was activated in the browser — load the selected file. */
static void on_browser_activate(FileBrowser *fb, const gchar *path, gpointer user_data) {
    FastNoteApp *app = user_data;
    (void)fb;
    dialog_close();

    FILE *f = fopen(path, "r");
    if (!f) {
        fn_set_error("Cannot open file: %s", path);
        gtk_label_set_text(GTK_LABEL(app->status_label), fn_error());
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    gchar *content = g_malloc(size + 1);
    if (size > 0) {
        fread(content, 1, size, f);
    }
    content[size] = '\0';
    fclose(f);

    if (!fastnote_set_document(app, path, content)) {
        g_free(content);
        gtk_label_set_text(GTK_LABEL(app->status_label), "Cannot open file");
        return;
    }
    g_free(content);
    fn_event(app, "open");
    gchar *msg = g_strdup_printf("Opened %s", path);
    gtk_label_set_text(GTK_LABEL(app->status_label), msg);
    g_free(msg);
}

/* Export save dialog: the user types or browses a destination path. */
static void export_dialog(FastNoteApp *app, gboolean pdf) {
    if (active_dialog) dialog_close();

    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), pdf ? "Export PDF" : "Export HTML");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 160);
    if (app->window) {
        gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(app->window));
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    }

    const gchar *defname = pdf ? "export.pdf" : "export.html";
    gchar *defpath = g_build_filename(app->notes_dir, defname, NULL);

    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), defpath);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    g_free(defpath);

    GtkWidget *ok_btn = gtk_button_new_with_label("Export");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(buttons), cancel_btn);
    gtk_box_append(GTK_BOX(buttons), ok_btn);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(vbox), entry);
    gtk_box_append(GTK_BOX(vbox), buttons);
    gtk_window_set_child(GTK_WINDOW(win), vbox);

    struct ExportDialogCtx *ctx = g_new0(struct ExportDialogCtx, 1);
    ctx->app = app;
    ctx->pdf = pdf;

    /* The OK button's real handler: read the entry, export, close. */
    active_dialog = win;
    dialog_app = app;
    g_object_set_data(G_OBJECT(win), "fn-export-ctx", ctx);
    g_object_set_data(G_OBJECT(win), "fn-export-entry", entry);
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(dialog_close), NULL);
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_export_confirm), ctx);
    g_signal_connect(entry, "activate", G_CALLBACK(on_export_confirm), ctx);
    add_dialog_keys(win, entry);
    g_signal_connect_swapped(win, "destroy", G_CALLBACK(g_free), ctx);

    gtk_window_present(GTK_WINDOW(win));
}

void on_export_confirm(GtkWidget *widget, gpointer user_data) {
    struct ExportDialogCtx *ctx = user_data;
    (void)widget;
    if (!ctx || !active_dialog) return;

    GtkWidget *entry = g_object_get_data(G_OBJECT(active_dialog), "fn-export-entry");
    if (!entry) return;
    const gchar *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;

    FastNoteApp *app = ctx->app;
    gboolean ok = FALSE;
    if (ctx->pdf) {
        ok = actions_export_pdf(app, path);
    } else {
        ok = actions_export_html(app, path);
    }
    dialog_close();
    if (ok) {
        fn_event(app, ctx->pdf ? "export-pdf" : "export-html");
        gtk_label_set_text(GTK_LABEL(app->status_label), "Export written");
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_label),
                           fn_error() ? fn_error() : "Export failed");
    }
}

void on_open_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    (void)widget;
    if (app) open_file_dialog(app);
}

void on_save_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    (void)widget;
    if (!app) return;
    if (!app->current_path) {
        /* No path yet — Save behaves as Save As (FR-5). */
        on_save_as_clicked(widget, user_data);
        return;
    }
    if (actions_save_file(app)) {
        fn_event(app, "save");
        gtk_label_set_text(GTK_LABEL(app->status_label), "Saved");
        fastnote_update_title(app);
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_label),
                           fn_error() ? fn_error() : "Save failed");
    }
}

void on_save_as_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    (void)widget;
    if (!app) return;

    if (active_dialog) dialog_close();
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Save As");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 160);
    if (app->window) {
        gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(app->window));
        gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    }

    gchar *basename = app->current_path ? g_path_get_basename(app->current_path) : NULL;
    gchar *defpath = g_build_filename(app->notes_dir, basename ? basename : "untitled.md", NULL);
    g_free(basename);
    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), defpath);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    g_free(defpath);

    GtkWidget *ok_btn = gtk_button_new_with_label("Save");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(buttons), cancel_btn);
    gtk_box_append(GTK_BOX(buttons), ok_btn);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(vbox), entry);
    gtk_box_append(GTK_BOX(vbox), buttons);
    gtk_window_set_child(GTK_WINDOW(win), vbox);

    active_dialog = win;
    dialog_app = app;
    g_object_set_data(G_OBJECT(win), "fn-export-entry", entry);
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(dialog_close), NULL);
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_save_as_confirm), app);
    g_signal_connect(entry, "activate", G_CALLBACK(on_save_as_confirm), app);
    add_dialog_keys(win, entry);
    gtk_window_present(GTK_WINDOW(win));
}

void on_save_as_confirm(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = user_data;
    (void)widget;
    if (!app || !active_dialog) return;
    GtkWidget *entry = g_object_get_data(G_OBJECT(active_dialog), "fn-export-entry");
    if (!entry) return;
    const gchar *path = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!path || !*path) return;

    gboolean ok = actions_save_as_file(app, path);
    dialog_close();
    if (ok) {
        fn_event(app, "save-as");
        fastnote_update_title(app);
        gtk_label_set_text(GTK_LABEL(app->status_label), "Saved as");
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_label),
                           fn_error() ? fn_error() : "Save As failed");
    }
}

void on_export_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    (void)widget;
    if (app) export_dialog(app, FALSE);
}

void on_export_pdf_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    (void)widget;
    if (app) export_dialog(app, TRUE);
}

void on_theme_clicked(GtkWidget *widget, gpointer user_data) {
    FastNoteApp *app = (FastNoteApp *)user_data;
    (void)widget;
    if (!app) return;
    app->theme = (app->theme + 1) % 2;
    const gchar *theme_name = app->theme == 0 ? "light" : "dark";
    gchar *msg = g_strdup_printf("Theme: %s", theme_name);
    gtk_label_set_text(GTK_LABEL(app->status_label), msg);
    g_free(msg);
}

void on_quit_clicked(GtkWidget *widget, gpointer user_data) {
    gtk_window_close(GTK_WINDOW(widget));
    (void)user_data;
}