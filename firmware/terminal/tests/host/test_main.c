#include <stdio.h>

#include "test_assert.h"

void test_ascii_renderer_run(void);
void test_builtin_themes_run(void);
void test_command_table_run(void);
void test_manifest_run(void);
void test_motion_profiles_run(void);
void test_runtime_helpers_run(void);
void test_scene_manager_run(void);
void test_state_adapter_run(void);
void test_theme_manager_run(void);
void test_tokens_run(void);
void test_view_model_run(void);

int g_test_checks = 0;
int g_test_failures = 0;

int main(void)
{
    printf("MeetAgent Terminal host tests\n");
    RUN(test_ascii_renderer_run);
    RUN(test_builtin_themes_run);
    RUN(test_command_table_run);
    RUN(test_manifest_run);
    RUN(test_motion_profiles_run);
    RUN(test_runtime_helpers_run);
    RUN(test_scene_manager_run);
    RUN(test_state_adapter_run);
    RUN(test_theme_manager_run);
    RUN(test_tokens_run);
    RUN(test_view_model_run);

    printf("\n%d checks, %d failure(s)\n", g_test_checks, g_test_failures);
    return g_test_failures == 0 ? 0 : 1;
}
