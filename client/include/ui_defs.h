#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <gtk/gtk.h>

// Global window pointer, managed by ui.c but accessed by components for destruction?
// Or better: ui.c manages the window pointer. Components just return the new window or set it.
// Let's keep the window pointer in ui.c and have components act on it, or pass it around.
// To follow previous pattern: ui.c holds the `static GtkWidget *window`.
// Components should probably manipulate a passed `GtkWidget **window` or similar.
// But simplify: ui.c provides a function "set_active_window(GtkWidget *w)"?

// Actually, `window_manager` helper is good.
// ui_login.c: calls create_window(), sets up widgets, returns the GtkWidget*?
// Then ui.c assigns it to its static variable.

// Let's assume ui_common for shared widgets like status label?
// Status label is tricky because it's replaced in every window.
// Maybe pass status label to controller?

// Re-thinking: ui.c acts as the mediator.
// ui_login.c: void create_login_screen(GtkWidget **window_ptr, GtkWidget **status_label_ptr);
// ui_home.c: void create_home_screen(GtkWidget **window_ptr, GtkWidget **status_label_ptr, const char *username);

#endif
