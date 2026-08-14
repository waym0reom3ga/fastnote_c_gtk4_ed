/* FastNote C/GTK4 Edition — Entry Point
 *
 * Exactly two permitted flags (spec §5.1):
 *   --version        print port identifier and version, exit 0
 *   --event-file P   append one line per completed user-visible phase
 *                    (painted/open/save/save-as/export-html/export-pdf)
 * Any other argument is rejected with a non-zero exit.
 */

#include <gtk/gtk.h>
#include "app.h"
#include <stdio.h>
#include <string.h>

static void print_usage(FILE *out) {
    fprintf(out,
        "fastnote_c_gtk4 — markdown editor (C/GTK4)\n"
        "usage: fastnote_c_gtk4 [--version] [--event-file PATH]\n"
        "  --version        print version and exit\n"
        "  --event-file P   append a phase marker line to P when each\n"
        "                   user-visible phase completes\n");
}

int main(int argc, char *argv[]) {
    const char *event_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("fastnote_c_gtk4 v1.1\n");
            return 0;
        }
        if (strcmp(argv[i], "--event-file") == 0 && i + 1 < argc) {
            event_file = argv[i + 1];
            i++;
            continue;
        }
        fprintf(stderr, "fastnote_c_gtk4: unknown option: %s\n", argv[i]);
        print_usage(stderr);
        return 2;
    }

    /* Pass only the program name and genuine GTK options to the application;
     * --event-file is ours, not GTK's. */
    char *run_argv[64];
    int n = 0;
    run_argv[n++] = argv[0];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--event-file") == 0) { i++; continue; }
        if (n < 63) run_argv[n++] = argv[i];
    }
    run_argv[n] = NULL;

    FastNoteApp *app = fastnote_app_new();
    if (event_file) {
        g_free(app->event_file);
        app->event_file = g_strdup(event_file);
    }
    int status = fastnote_app_run(app, n, run_argv);
    fastnote_app_free(app);
    return status;
}
