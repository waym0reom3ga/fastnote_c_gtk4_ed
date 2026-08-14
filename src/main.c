/* FastNote C/GTK4 Edition — Entry Point */

#include <gtk/gtk.h>
#include "app.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
    /* The only permitted flag: --version (specification §5.1). */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("fastnote_c_gtk4 v1.0\n");
            return 0;
        }
    }

    FastNoteApp *app = fastnote_app_new();
    int status = fastnote_app_run(app, argc, argv);
    fastnote_app_free(app);
    return status;
}