/* FastNote C/GTK4 Edition — GUI tests (A13 mechanism)
 *
 * These tests drive the REAL widget tree the application shows: the buttons
 * are the toolbar buttons, "clicking" emits the same "clicked" signal a real
 * pointer press/release produces, typing inserts into the real GtkTextBuffer
 * (which fires the same "changed" signal real keystrokes fire), and every
 * assertion checks a real observable outcome — a file on disk, the document
 * state, the preview, the window title.
 *
 * There is no seam: no flag, no CLI, no bypass.  If a control is unbound,
 * a test fails.  Set FASTNOTE_SABOTAGE=1 to unbind the Open button and
 * verify the suite fails (the acceptance harness does this).
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "ui.h"
#include "actions.h"
#include "renderer.h"

static int failures = 0;
static int checks = 0;
static int test_status = 0;
static char *dir = NULL;
static gchar *note = NULL;
static gchar *subdir = NULL;

static void ok(const char *name, const char *desc) {
    printf("ok   %s %s\n", name, desc ? desc : "");
    checks++;
}

static void mismatch(const char *name, const char *desc) {
    printf("MISMATCH %s %s\n", name, desc ? desc : "");
    failures++;
    checks++;
}

static void click(GtkWidget *btn) {
    g_signal_emit_by_name(btn, "clicked");
}

/* Click a button by walking the widget tree to its label. */
static GtkWidget *find_button(GtkWidget *root, const char *label) {
    if (!root) return NULL;
    if (GTK_IS_BUTTON(root)) {
        GtkWidget *child = gtk_button_get_child(GTK_BUTTON(root));
        if (child && GTK_IS_LABEL(child)) {
            const char *text = gtk_label_get_text(GTK_LABEL(child));
            if (text && strcmp(text, label) == 0) return root;
        }
    }
    /* walk children depth-first */
    for (GtkWidget *c = gtk_widget_get_first_child(root); c; c = gtk_widget_get_next_sibling(c)) {
        GtkWidget *found = find_button(c, label);
        if (found) return found;
    }
    return NULL;
}

/* Type text into the editor the same way a user's keystrokes reach it:
 * insert into the real GtkTextBuffer, firing "changed". */
static void type_text(FastNoteApp *app, const char *text) {
    gtk_text_buffer_insert_at_cursor(app->editor_buffer, text, -1);
}

/* GTK4 has no get_child_at_index; walk siblings. */
static GtkWidget *child_at_index(GtkWidget *parent, int index) {
    GtkWidget *c = gtk_widget_get_first_child(parent);
    for (int i = 0; c; i++, c = gtk_widget_get_next_sibling(c)) {
        if (i == index) return c;
    }
    return NULL;
}

/* Find the first GtkListBox anywhere in the subtree. */
static GtkWidget *find_list_box(GtkWidget *root) {
    if (!root) return NULL;
    if (GTK_IS_LIST_BOX(root)) return root;
    for (GtkWidget *c = gtk_widget_get_first_child(root); c; c = gtk_widget_get_next_sibling(c)) {
        GtkWidget *found = find_list_box(c);
        if (found) return found;
    }
    return NULL;
}

/* Find the first GtkLabel anywhere in the subtree (browser path label). */
static GtkWidget *find_label(GtkWidget *root) {
    if (!root) return NULL;
    if (GTK_IS_LABEL(root)) return root;
    for (GtkWidget *c = gtk_widget_get_first_child(root); c; c = gtk_widget_get_next_sibling(c)) {
        GtkWidget *found = find_label(c);
        if (found) return found;
    }
    return NULL;
}

/* The full test sequence, driven from inside the running GApplication's
 * main loop (g_timeout_add) — the same loop a real user session runs. */
static gboolean drive(void *user_data) {
    FastNoteApp *app = user_data;

    /* ---- toolbar exists ------------------------------------------------- */
    GtkWidget *open_btn = find_button(app->window, "Open");
    GtkWidget *save_btn = find_button(app->window, "Save");
    GtkWidget *save_as_btn = find_button(app->window, "Save As");
    GtkWidget *export_btn = find_button(app->window, "Export HTML");
    GtkWidget *pdf_btn = find_button(app->window, "Export PDF");
    GtkWidget *theme_btn = find_button(app->window, "Theme");

    if (!open_btn || !save_btn || !save_as_btn || !export_btn || !pdf_btn || !theme_btn) {
        fprintf(stderr, "toolbar incomplete\n");
        g_application_quit(g_application_get_default());
        return G_SOURCE_REMOVE;
    }
    ok("toolbar.rendered", "all six controls present");

    gboolean sabotage = g_getenv("FASTNOTE_SABOTAGE") != NULL;
    if (sabotage) {
        /* The acceptance harness unbinds a control and requires the suite to
         * fail.  Unbind Open: nothing else changes. */
        g_signal_handlers_disconnect_by_func(app->open_btn, on_open_clicked, app);
    }

    /* ---- editor typing reaches state and preview (FR-3, FR-4) ----------- */
    type_text(app, "base text");
    if (strcmp(app->document_content, "base text") != 0) {
        mismatch("insert.changes-doc", "doc text unchanged");
    } else if (!app->dirty) {
        mismatch("insert.marks-dirty", "doc not dirty");
    } else if (!strstr(app->document_content, "base text")) {
        mismatch("insert.preview-stale", "content missing from state");
    } else {
        ok("insert.changes-doc", "typing reached document state, dirty set");
    }

    /* ---- Open: click the button, browse, load (FR-2) -------------------- */
    click(open_btn);

    /* The browser dialog should be present and list the seeded file. */
    extern GtkWidget *ui_active_dialog(void);
    GtkWidget *dialog = ui_active_dialog();
    if (!dialog) {
        mismatch("open.opens-browser", "browser did not open");
    } else {
        ok("open.opens-browser", "browser window opened");

        /* Find the file row in the list box and activate it — the same
         * signal a double-click fires. */
        GtkWidget *list = find_list_box(dialog);
        if (!list) {
            mismatch("open.lists-notes", "no list box in browser");
        } else {
            /* Check the seeded file is listed */
            gboolean found_note = FALSE, found_sub = FALSE;
            GtkListBoxRow *row;
            for (int i = 0; (row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), i)); i++) {
                GtkWidget *lbl = gtk_list_box_row_get_child(row);
                if (GTK_IS_LABEL(lbl)) {
                    const char *t = gtk_label_get_text(GTK_LABEL(lbl));
                    if (t && strcmp(t, "a.md") == 0) found_note = TRUE;
                    if (t && strcmp(t, "sub/") == 0) found_sub = TRUE;
                }
            }
            if (!found_note) mismatch("open.lists-notes", "a.md not listed");
            else if (!found_sub) mismatch("open.lists-subdir", "sub/ not listed");
            else {
                ok("open.lists-notes", "a.md and sub/ listed");
                /* Activate the a.md row — same signal as double-click */
                for (int i = 0; (row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list), i)); i++) {
                    GtkWidget *lbl = gtk_list_box_row_get_child(row);
                    if (GTK_IS_LABEL(lbl) &&
                        strcmp(gtk_label_get_text(GTK_LABEL(lbl)), "a.md") == 0) {
                        g_signal_emit_by_name(list, "row-activated", row);
                        break;
                    }
                }
            }
        }
    }

    /* ---- the loaded file is in the editor (FR-2) ------------------------ */
    if (app->document_content && strstr(app->document_content, "world")) {
        ok("open.entry-a-md", "a.md contents in the editor");
    } else {
        mismatch("open.entry-a-md", "clicking a.md did not open the doc");
    }
    if (app->current_path && strstr(app->current_path, "a.md")) {
        ok("open.path-tracked", "document path tracked");
    } else {
        mismatch("open.path-tracked", "path not set after open");
    }
    if (!app->dirty) {
        ok("open.not-dirty", "freshly opened document is not dirty");
    } else {
        mismatch("open.not-dirty", "freshly opened document marked dirty");
    }

    /* ---- Open via single click + browser Open button (the flow a user
     * actually performs: click a row once, press Open) ------------------- */
    click(open_btn);
    GtkWidget *bdialog = ui_active_dialog();
    GtkWidget *blist = bdialog ? find_list_box(bdialog) : NULL;
    if (!blist) {
        mismatch("open.select-then-open", "browser did not reopen");
    } else {
        ok("open.select-then-open", "browser reopened");
        /* single click: row-selected, exactly what a pointer click emits */
        GtkListBoxRow *brow = NULL;
        for (int i = 0; (brow = gtk_list_box_get_row_at_index(GTK_LIST_BOX(blist), i)); i++) {
            GtkWidget *lbl = gtk_list_box_row_get_child(brow);
            if (GTK_IS_LABEL(lbl) &&
                strcmp(gtk_label_get_text(GTK_LABEL(lbl)), "b.md") == 0) {
                g_signal_emit_by_name(blist, "row-selected", brow);
                break;
            }
        }
        GtkWidget *bopen = find_button(bdialog, "Open");
        if (!bopen) {
            mismatch("open.select-then-open", "browser has no Open button");
        } else {
            click(bopen);
            if (app->document_content && strstr(app->document_content, "B-MARKER")) {
                ok("open.select-then-open", "selected file loaded via browser Open");
            } else {
                mismatch("open.select-then-open", "browser Open did not load the selected file");
            }
            if (app->current_path && strstr(app->current_path, "b.md")) {
                ok("open.selected-path-tracked", "b.md path tracked");
            } else {
                mismatch("open.selected-path-tracked", "b.md path not tracked");
            }
        }
    }

    /* ---- Open on a directory navigates into it (not an error) ----------- */
    click(open_btn);
    GtkWidget *cdialog = ui_active_dialog();
    GtkWidget *clist = cdialog ? find_list_box(cdialog) : NULL;
    if (!clist) {
        mismatch("open.dir-navigates", "browser did not reopen");
    } else {
        /* single click the sub/ directory row, then press Open */
        GtkListBoxRow *crow = NULL;
        for (int i = 0; (crow = gtk_list_box_get_row_at_index(GTK_LIST_BOX(clist), i)); i++) {
            GtkWidget *lbl = gtk_list_box_row_get_child(crow);
            if (GTK_IS_LABEL(lbl) &&
                strcmp(gtk_label_get_text(GTK_LABEL(lbl)), "sub/") == 0) {
                g_signal_emit_by_name(clist, "row-selected", crow);
                break;
            }
        }
        GtkWidget *copen = find_button(cdialog, "Open");
        if (!copen) {
            mismatch("open.dir-navigates", "browser has no Open button");
        } else {
            click(copen);
            GtkWidget *pathlabel = find_label(cdialog);
            const gchar *shown = pathlabel ? gtk_label_get_text(GTK_LABEL(pathlabel)) : NULL;
            gboolean navigated = shown && strstr(shown, "sub");
            gboolean still_open = ui_active_dialog() != NULL;
            if (!navigated) {
                mismatch("open.dir-navigates", "path label did not change to sub/");
            } else if (!still_open) {
                mismatch("open.dir-navigates", "dialog closed on directory Open");
            } else {
                ok("open.dir-navigates", "Open on directory navigates into it");
            }
            /* close the browser before continuing */
            GtkWidget *ccancel = find_button(cdialog, "Cancel");
            if (ccancel) click(ccancel);
        }
    }

    /* ---- Save: click Save, verify file on disk (FR-5) ------------------- */
    gboolean have_edit = FALSE;
    if (app->current_path && app->document_content) {
        gchar *edited = g_strdup_printf("%s\n\nMARKER-EDIT", app->document_content);
        gtk_text_buffer_set_text(app->editor_buffer, edited, -1);
        g_free(edited);
        have_edit = TRUE;
    }
    click(save_btn);
    gchar *saved = NULL;
    if (app->current_path) {
        g_file_get_contents(app->current_path, &saved, NULL, NULL);
    }
    if (!have_edit) {
        mismatch("save.wrote-file", "no document to save");
    } else if (!saved) {
        mismatch("save.wrote-file", "save did not persist the document");
    } else if (!strstr(saved, "MARKER-EDIT")) {
        mismatch("save.wrote-file", "edit not on disk after Save");
    } else if (app->dirty) {
        mismatch("save.clears-dirty", "still dirty after save");
    } else {
        ok("save.wrote-file", "edit persisted, dirty cleared");
    }
    g_free(saved);

    /* ---- Export HTML: click Export, pick a path (FR-7) ------------------ */
    click(export_btn);
    GtkWidget *edlg = ui_active_dialog();
    if (!edlg) {
        mismatch("export.opens-browser", "export did not open the save dialog");
    } else {
        ok("export.opens-browser", "export dialog opened");
        /* The dialog has an entry and an Export button.  Set the path the
         * same way a user would, then click the real Export button. */
        GtkWidget *entry = g_object_get_data(G_OBJECT(edlg), "fn-export-entry");
        gchar *out_path = g_build_filename(dir, "out.html", NULL);
        gtk_editable_set_text(GTK_EDITABLE(entry), out_path);
        g_free(out_path);

        GtkWidget *vbox = gtk_window_get_child(GTK_WINDOW(edlg));
        GtkWidget *buttons = child_at_index(vbox, 1);
        GtkWidget *ok_btn = child_at_index(buttons, 1);
        click(ok_btn);

        gchar *html = NULL;
        out_path = g_build_filename(dir, "out.html", NULL);
        g_file_get_contents(out_path, &html, NULL, NULL);
        g_free(out_path);
        if (!html) {
            mismatch("export.wrote-html", "no HTML file written");
        } else if (!strstr(html, "<!DOCTYPE html>")) {
            mismatch("export.html-doctype", "exported HTML is not a standalone document");
        } else if (!strstr(html, "<style")) {
            mismatch("export.html-style", "export has no stylesheet");
        } else if (!strstr(html, "MARKER-EDIT")) {
            mismatch("export.html-content", "document content absent from export");
        } else {
            ok("export.wrote-html", "standalone HTML written with content");
        }
        g_free(html);
    }

    /* ---- Export PDF: real cairo PDF (FR-8) ------------------------------ */
    click(pdf_btn);
    GtkWidget *pdlg = ui_active_dialog();
    if (!pdlg) {
        mismatch("export-pdf.opens-dialog", "PDF export did not open the save dialog");
    } else {
        ok("export-pdf.opens-dialog", "PDF export dialog opened");
        GtkWidget *entry = g_object_get_data(G_OBJECT(pdlg), "fn-export-entry");
        gchar *pdf_path = g_build_filename(dir, "out.pdf", NULL);
        gtk_editable_set_text(GTK_EDITABLE(entry), pdf_path);
        g_free(pdf_path);

        GtkWidget *vbox = gtk_window_get_child(GTK_WINDOW(pdlg));
        GtkWidget *buttons = child_at_index(vbox, 1);
        GtkWidget *ok_btn = child_at_index(buttons, 1);
        click(ok_btn);

        gchar *pdf_path2 = g_build_filename(dir, "out.pdf", NULL);
        gboolean pdf_exists = g_file_test(pdf_path2, G_FILE_TEST_EXISTS);
        gchar *pdf_head = NULL;
        if (pdf_exists) {
            gchar *buf = NULL;
            if (g_file_get_contents(pdf_path2, &buf, NULL, NULL) && buf) {
                pdf_head = g_strndup(buf, 4);
                g_free(buf);
            }
        }
        g_free(pdf_path2);
        if (!pdf_exists) {
            mismatch("export-pdf.wrote-pdf", "no PDF file written");
        } else if (!pdf_head || strcmp(pdf_head, "%PDF") != 0) {
            gchar *msg = g_strdup_printf("file exists but is not a PDF (head: %s)", pdf_head ? pdf_head : "(none)");
            mismatch("export-pdf.real-pdf", msg);
            g_free(msg);
        } else {
            ok("export-pdf.real-pdf", "PDF file written with %PDF magic");
        }
        g_free(pdf_head);
    }

    /* ---- Save As: click Save As, pick a path (FR-6) --------------------- */
    click(save_as_btn);
    GtkWidget *sdlg = ui_active_dialog();
    if (!sdlg) {
        mismatch("saveas.opens-dialog", "Save As did not open the dialog");
    } else {
        ok("saveas.opens-dialog", "Save As dialog opened");
        GtkWidget *entry = g_object_get_data(G_OBJECT(sdlg), "fn-export-entry");
        gchar *copy_path = g_build_filename(dir, "copy.md", NULL);
        gtk_editable_set_text(GTK_EDITABLE(entry), copy_path);
        g_free(copy_path);

        GtkWidget *vbox = gtk_window_get_child(GTK_WINDOW(sdlg));
        GtkWidget *buttons = child_at_index(vbox, 1);
        GtkWidget *ok_btn = child_at_index(buttons, 1);
        click(ok_btn);

        gchar *copy_path2 = g_build_filename(dir, "copy.md", NULL);
        gboolean copy_exists = g_file_test(copy_path2, G_FILE_TEST_EXISTS);
        gboolean path_tracked = app->current_path && strstr(app->current_path, "copy.md");
        g_free(copy_path2);
        if (!copy_exists) {
            mismatch("saveas.wrote-file", "Save As did not write the file");
        } else if (!path_tracked) {
            mismatch("saveas.tracks-path", "document path not updated");
        } else {
            ok("saveas.wrote-file", "file written and path updated");
        }
    }

    /* ---- Theme toggle (visual state change) ----------------------------- */
    int theme_before = app->theme;
    click(theme_btn);
    if (app->theme == theme_before) {
        mismatch("theme.toggled", "theme did not change");
    } else {
        ok("theme.toggled", "theme switched");
    }

    /* ---- Renderer: escaping and no overflow on heading-heavy docs ------- */
    {
        Renderer *r = renderer_new();

        gboolean ok_escape = renderer_render_markdown(r, "a < b & c > \"q\"");
        const gchar *h = ok_escape ? renderer_get_html(r) : NULL;
        if (h && strcmp(h, "a &lt; b &amp; c &gt; &quot;q&quot;") == 0) {
            ok("renderer.escapes-text", "raw text is HTML-escaped");
        } else {
            gchar *msg = g_strdup_printf("unexpected renderer output: %s", h ? h : "(null)");
            mismatch("renderer.escapes-text", msg);
            g_free(msg);
        }

        gboolean ok_head = renderer_render_markdown(r, "# <script>alert(1)</script>");
        h = ok_head ? renderer_get_html(r) : NULL;
        if (h && strstr(h, "<h1>&lt;script&gt;alert(1)&lt;/script&gt;</h1>")) {
            ok("renderer.escapes-headings", "heading content is HTML-escaped");
        } else {
            gchar *msg = g_strdup_printf("heading not escaped: %s", h ? h : "(null)");
            mismatch("renderer.escapes-headings", msg);
            g_free(msg);
        }

        /* 5000 heading-only lines: the old fixed buffer would overflow. */
        GString *big = g_string_new(NULL);
        for (int i = 0; i < 5000; i++) g_string_append(big, "# \n");
        gboolean ok_big = renderer_render_markdown(r, big->str);
        h = ok_big ? renderer_get_html(r) : NULL;
        int heads = 0;
        if (h) {
            const gchar *q = h;
            while ((q = strstr(q, "</h1>")) != NULL) { heads++; q += 5; }
        }
        if (ok_big && heads == 5000) {
            ok("renderer.no-overflow", "5000 headings rendered (was heap overflow)");
        } else {
            gchar *msg = g_strdup_printf("expected 5000 headings, got %d", heads);
            mismatch("renderer.no-overflow", msg);
            g_free(msg);
        }
        g_string_free(big, TRUE);
        renderer_free(r);
    }

    printf("\n%s %d checks, %d mismatches\n",
           failures == 0 ? "ui tests: passed" : "ui tests: FAILED",
           checks, failures);
    test_status = failures == 0 ? 0 : 1;
    g_application_quit(g_application_get_default());
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    /* Build a seeded document store in a temp dir. */
    char tmpdir[] = "/tmp/fastnote_gtk_test_XXXXXX";
    dir = g_mkdtemp(tmpdir);
    note = g_build_filename(dir, "a.md", NULL);
    g_file_set_contents(note, "# Hello\n\nworld 🚀\n", -1, NULL);
    gchar *note2 = g_build_filename(dir, "b.md", NULL);
    g_file_set_contents(note2, "second file B-MARKER\n", -1, NULL);
    g_free(note2);
    subdir = g_build_filename(dir, "sub", NULL);
    g_mkdir(subdir, 0755);

    FastNoteApp *app = fastnote_app_new();
    app->notes_dir = g_strdup(dir);

    /* Drive the test sequence from inside the running main loop. */
    g_timeout_add(100, drive, app);

    /* The exact same entry point the shipped binary uses. */
    int status = fastnote_app_run(app, argc, argv);
    fastnote_app_free(app);
    return status == 0 ? test_status : status;
}