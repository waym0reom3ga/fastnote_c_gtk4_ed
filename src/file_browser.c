/* FastNote C/GTK4 Edition — File browser component */

#include "file_browser.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

struct _FileBrowser {
    gchar *current_path;
    GList *file_list;
    GtkWidget *list_view;
    GtkWidget *path_entry;
};

static void file_browser_refresh(FileBrowser *fb) {
    if (!fb || !fb->current_path) return;
    
    DIR *dir = opendir(fb->current_path);
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files */
        if (entry->d_name[0] == '.') continue;
        
        gchar *full_path = g_build_filename(fb->current_path, entry->d_name, NULL);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            /* Add to list */
            fb->file_list = g_list_append(fb->file_list, full_path);
        } else {
            g_free(full_path);
        }
    }
    
    closedir(dir);
}

FileBrowser *file_browser_new(void) {
    FileBrowser *fb = g_malloc0(sizeof(FileBrowser));
    fb->current_path = g_strdup(g_get_home_dir());
    return fb;
}

void file_browser_free(FileBrowser *fb) {
    if (!fb) return;
    
    /* Free all paths in the list */
    for (GList *l = fb->file_list; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_list_free(fb->file_list);
    g_free(fb->current_path);
    g_free(fb);
}

void file_browser_set_start_dir(FileBrowser *fb, const char *dir) {
    if (fb && dir) {
        g_free(fb->current_path);
        fb->current_path = g_strdup(dir);
    }
}

const gchar *file_browser_get_selected_path(FileBrowser *fb) {
    if (!fb || !fb->file_list) return NULL;
    
    /* Return the first item in the list (simplified — real impl would track selection) */
    return fb->file_list->data;
}

gboolean file_browser_is_cancelled(FileBrowser *fb) {
    return fb && fb->cancelled;
}

GtkWidget *file_browser_get_widget(FileBrowser *fb) {
    if (!fb) return NULL;
    
    /* Create a simple list view */
    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    
    /* Add files to the list */
    for (GList *l = fb->file_list; l != NULL; l = l->next) {
        const gchar *path = (const gchar *)l->data;
        const gchar *name = g_path_get_basename(path);
        
        GtkWidget *row = gtk_label_new(name);
        gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
    }
    
    return list;
}
