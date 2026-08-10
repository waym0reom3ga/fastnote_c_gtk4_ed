/* FastNote C/GTK4 Edition — Entry Point */

#include <gtk/gtk.h>
#include "app.h"
#include "file_browser.h"
#include "renderer.h"
#include "export.h"
#include "actions.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    /* Check for --version before any GTK init */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("fastnote-c-gtk4 v1.0\n");
            return 0;
        }
    }
    
    FastNoteApp *app = fastnote_app_new();
    
    int status = fastnote_app_run(app, argc, argv);
    
    g_object_unref(app);
    return status;
}
