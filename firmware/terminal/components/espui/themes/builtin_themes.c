#include "espui_theme.h"

void espui_register_builtin_themes(void)
{
    const espui_theme_descriptor_t *safe = espui_theme_safe();
    const espui_theme_descriptor_t *terminal = espui_theme_terminal_aesthetic();

    if (safe != NULL && safe->manifest != NULL &&
        espui_theme_registry_find(safe->manifest->theme_id) == NULL) {
        espui_theme_registry_add(safe);
    }
    if (terminal != NULL && terminal->manifest != NULL &&
        espui_theme_registry_find(terminal->manifest->theme_id) == NULL) {
        espui_theme_registry_add(terminal);
    }
}
