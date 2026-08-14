/* FastNote C/GTK4 Edition — File browser component */

#include "file_browser.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

struct _FileBrowser {
    gchar *current_dir;
    GList *entries;          /* list of gchar* full paths (files and dirs) */
    gint selected_index;     /* index into entries of the selected row, -1 = none */
    GtkWidget *list;         /* GtkListBox */
    GtkWidget *path_entry;   /* GtkEntry showing the current dir / typed path */
    GtkWidget *widget;       /* top-level vbox */
    FileBrowserActivateFn activate_fn;
    gpointer activate_data;
};

static void fb_rebuild_list(FileBrowser *fb);

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    FileBrowser *fb = user_data;
    gint idx = gtk_list_box_row_get_index(row);
    GList *entry = g_list_nth(fb->entries, idx);
    if (!entry || !entry->data) return;

    const gchar *path = entry->data;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* double-clicking a directory navigates into it */
        g_free(fb->current_dir);
        fb->current_dir = g_strdup(path);
        fb_rebuild_list(fb);
    } else if (fb->activate_fn) {
        fb->activate_fn(fb, path, fb->activate_data);
    }
}

/* A row was selected (single click or keyboard): remember it so the Open
 * button can act on it. */
static void on_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    FileBrowser *fb = user_data;
    fb->selected_index = row ? gtk_list_box_row_get_index(row) : -1;
}

static void on_open_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    FileBrowser *fb = user_data;
    const gchar *path = file_browser_selected_path(fb);
    if (!path) return;
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* Open on a directory navigates into it, like a double-click */
        g_free(fb->current_dir);
        fb->current_dir = g_strdup(path);
        fb_rebuild_list(fb);
    } else if (fb->activate_fn) {
        fb->activate_fn(fb, path, fb->activate_data);
    }
}

static void on_up_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    file_browser_parent((FileBrowser *)user_data);
}

static void fb_rebuild_list(FileBrowser *fb) {
    gtk_editable_set_text(GTK_EDITABLE(fb->path_entry), fb->current_dir);

    /* clear old rows and entries */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(fb->list)) != NULL) {
        gtk_widget_unparent(child);
    }
    g_list_free_full(fb->entries, g_free);
    fb->entries = NULL;
    fb->selected_index = -1;

    DIR *dir = opendir(fb->current_dir);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_name[0] == '.') {
            continue; /* skip hidden files */
        }

        gchar *full = g_build_filename(fb->current_dir, entry->d_name, NULL);
        struct stat st;
        if (stat(full, &st) != 0) {
            g_free(full);
            continue;
        }

        const gchar *suffix = S_ISDIR(st.st_mode) ? "/" : "";
        gchar *display = g_strdup_printf("%s%s", entry->d_name, suffix);
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(display);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_list_box_append(GTK_LIST_BOX(fb->list), row);
        g_free(display);

        fb->entries = g_list_append(fb->entries, full);
    }
    closedir(dir);
}

/* The path entry (spec §3.2): Enter resolves the typed path. A directory
 * navigates into it; a file is opened through the normal activate callback;
 * a bare name is tried relative to the current directory. */
static void on_path_activate(GtkEntry *entry, gpointer user_data) {
    FileBrowser *fb = user_data;
    const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!text || !*text) return;

    struct stat st;
    if (stat(text, &st) == 0 && S_ISDIR(st.st_mode)) {
        file_browser_set_dir(fb, text);
        return;
    }
    if (stat(text, &st) == 0 && !S_ISDIR(st.st_mode) && fb->activate_fn) {
        fb->activate_fn(fb, text, fb->activate_data);
        return;
    }
    gchar *full = g_build_filename(fb->current_dir, text, NULL);
    if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
        file_browser_set_dir(fb, full);
    } else if (stat(full, &st) == 0 && !S_ISDIR(st.st_mode) && fb->activate_fn) {
        fb->activate_fn(fb, full, fb->activate_data);
    }
    g_free(full);
}

FileBrowser *file_browser_new(const gchar *start_dir) {
    FileBrowser *fb = g_malloc0(sizeof(FileBrowser));
    fb->current_dir = g_strdup(start_dir ? start_dir : g_get_home_dir());

    fb->path_entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(fb->path_entry), TRUE);
    g_signal_connect(fb->path_entry, "activate", G_CALLBACK(on_path_activate), fb);

    GtkWidget *scroll = gtk_scrolled_window_new();
    fb->list = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), fb->list);
    gtk_widget_set_vexpand(fb->list, TRUE);

    GtkWidget *open_btn = gtk_button_new_with_label("Open");
    GtkWidget *up_btn = gtk_button_new_with_label("Up");
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(buttons), up_btn);
    gtk_box_append(GTK_BOX(buttons), open_btn);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(vbox), fb->path_entry);
    gtk_box_append(GTK_BOX(vbox), scroll);
    gtk_box_append(GTK_BOX(vbox), buttons);
    gtk_widget_set_vexpand(scroll, TRUE);

    g_signal_connect(fb->list, "row-activated", G_CALLBACK(on_row_activated), fb);
    g_signal_connect(fb->list, "row-selected", G_CALLBACK(on_row_selected), fb);
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_clicked), fb);
    g_signal_connect(up_btn, "clicked", G_CALLBACK(on_up_clicked), fb);

    fb->widget = vbox;
    fb_rebuild_list(fb);
    return fb;
}

void file_browser_free(FileBrowser *fb) {
    if (!fb) return;
    g_free(fb->current_dir);
    g_list_free_full(fb->entries, g_free);
    g_free(fb);
}

gboolean file_browser_enter(FileBrowser *fb, const gchar *name) {
    if (!fb || !name) return FALSE;
    gchar *full = g_build_filename(fb->current_dir, name, NULL);
    struct stat st;
    if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) {
        g_free(full);
        return FALSE;
    }
    g_free(fb->current_dir);
    fb->current_dir = full;
    fb_rebuild_list(fb);
    return TRUE;
}

gboolean file_browser_parent(FileBrowser *fb) {
    if (!fb) return FALSE;
    gchar *parent = g_path_get_dirname(fb->current_dir);
    gboolean ok = (strcmp(parent, fb->current_dir) != 0);
    g_free(fb->current_dir);
    fb->current_dir = parent;
    if (ok) fb_rebuild_list(fb);
    return ok;
}

gboolean file_browser_set_dir(FileBrowser *fb, const gchar *dir) {
    if (!fb || !dir) return FALSE;
    g_free(fb->current_dir);
    fb->current_dir = g_strdup(dir);
    fb_rebuild_list(fb);
    return TRUE;
}

const gchar *file_browser_current_dir(FileBrowser *fb) {
    return fb ? fb->current_dir : NULL;
}

const gchar *file_browser_selected_path(FileBrowser *fb) {
    if (!fb || fb->selected_index < 0) return NULL;
    GList *entry = g_list_nth(fb->entries, fb->selected_index);
    return (entry && entry->data) ? entry->data : NULL;
}

GtkWidget *file_browser_get_widget(FileBrowser *fb) {
    return fb ? fb->widget : NULL;
}

GtkWidget *file_browser_get_entry(FileBrowser *fb) {
    return fb ? fb->path_entry : NULL;
}

void file_browser_set_activate_cb(FileBrowser *fb, FileBrowserActivateFn fn,
                                  gpointer user_data) {
    fb->activate_fn = fn;
    fb->activate_data = user_data;
}