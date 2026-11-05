#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ascii_renderer.h"
#include "espui_view_model.h"

static const espui_visual_state_t ALL_STATES[] = {
    ESPUI_VISUAL_IDLE,     ESPUI_VISUAL_WORKING, ESPUI_VISUAL_COMPLETED,
    ESPUI_VISUAL_APPROVAL, ESPUI_VISUAL_ERROR,   ESPUI_VISUAL_OFFLINE,
};

static void model_base(espui_view_model_t *model)
{
    espui_view_model_init(model);
    model->node.status = ESPUI_NODE_ONLINE;
    model->node.connected = true;
    strcpy(model->node.node_id, "node-demo");
    strcpy(model->node.display_name, "Demo Node");
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    if (agent != NULL) {
        strcpy(agent->display_name, "OpenCode Main");
        agent->status = ESPUI_AGENT_IDLE;
    }
}

static void add_working(espui_view_model_t *model)
{
    espui_agent_view_t *agent =
        espui_view_model_upsert_agent(model, "opencode-main");
    agent->status = ESPUI_AGENT_BUSY;
    agent->active_session_count = 1;
    espui_session_view_t *session =
        espui_view_model_upsert_session(model, "ses-demo");
    session->status = ESPUI_SESSION_RUNNING;
    strcpy(session->agent_id, "opencode-main");
    strcpy(session->title, "MeetAgent Terminal");
}

static void build_model(espui_visual_state_t state, espui_view_model_t *model)
{
    model_base(model);
    switch (state) {
    case ESPUI_VISUAL_IDLE:
        break;
    case ESPUI_VISUAL_WORKING:
        add_working(model);
        break;
    case ESPUI_VISUAL_COMPLETED: {
        espui_agent_view_t *agent =
            espui_view_model_upsert_agent(model, "opencode-main");
        agent->status = ESPUI_AGENT_IDLE;
        (void)espui_view_model_push_attention(model, "done-demo",
                                              ESPUI_ATTENTION_COMPLETION);
        break;
    }
    case ESPUI_VISUAL_APPROVAL:
        add_working(model);
        (void)espui_view_model_push_attention(model, "per-demo",
                                              ESPUI_ATTENTION_APPROVAL);
        break;
    case ESPUI_VISUAL_ERROR: {
        espui_agent_view_t *agent =
            espui_view_model_upsert_agent(model, "opencode-main");
        agent->status = ESPUI_AGENT_ERROR;
        (void)espui_view_model_push_attention(model, "err-demo",
                                              ESPUI_ATTENTION_ERROR);
        break;
    }
    case ESPUI_VISUAL_OFFLINE:
    default:
        model->node.status = ESPUI_NODE_OFFLINE;
        model->node.connected = false;
        espui_view_model_upsert_agent(model, "opencode-main")->status =
            ESPUI_AGENT_OFFLINE;
        break;
    }
}

static void print_frame(const espui_ascii_renderer_t *renderer, uint32_t time_ms)
{
    const espui_ascii_motion_t *motion =
        espui_ascii_renderer_motion(renderer, renderer->status.state);
    printf("=== %s  t=%ums  speed=%u%% density=%u%% amp=%u%%  frame=%u ===\n",
           espui_visual_state_label(renderer->status.state), (unsigned)time_ms,
           motion != NULL ? (unsigned)motion->speed_percent : 0u,
           motion != NULL ? (unsigned)motion->density_percent : 0u,
           motion != NULL ? (unsigned)motion->amplitude_percent : 0u,
           (unsigned)renderer->frame_count);
    for (int y = 0; y < renderer->frame.rows; ++y) {
        printf("%s\n", renderer->frame.cells[y]);
    }
    printf("\n");
    fflush(stdout);
}

static int parse_state(const char *name, espui_visual_state_t *out)
{
    for (size_t i = 0; i < sizeof(ALL_STATES) / sizeof(ALL_STATES[0]); ++i) {
        if (strcmp(name, espui_visual_state_name(ALL_STATES[i])) == 0) {
            *out = ALL_STATES[i];
            return 0;
        }
    }
    return -1;
}

static void preview_state(espui_ascii_renderer_t *renderer,
                          espui_visual_state_t state, unsigned frames,
                          uint32_t delay_ms)
{
    espui_view_model_t model;
    build_model(state, &model);

    espui_ascii_renderer_apply_view_model(renderer, &model,
                                          state == ESPUI_VISUAL_APPROVAL ||
                                                  state == ESPUI_VISUAL_ERROR ||
                                                  state == ESPUI_VISUAL_COMPLETED
                                              ? ESPUI_SCENE_ATTENTION
                                              : ESPUI_SCENE_AMBIENT);
    uint32_t time_ms = 0;
    print_frame(renderer, time_ms);
    for (unsigned i = 1; i < frames; ++i) {
        espui_ascii_renderer_advance(renderer, delay_ms);
        espui_ascii_renderer_render(renderer);
        time_ms += delay_ms;
        print_frame(renderer, time_ms);
    }
}

int main(int argc, char **argv)
{
    const char *state_name = argc > 1 ? argv[1] : "all";
    unsigned frames = argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 2;
    uint32_t delay_ms = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 400;
    if (frames == 0) {
        frames = 1;
    }

    espui_ascii_renderer_t renderer;
    espui_ascii_config_t config = {
        .cols = 60,
        .rows = 22,
        .glyph_ramp = ESPUI_ASCII_DEFAULT_RAMP,
        .target_fps = 15,
        .completed_pulse_ms = 1200,
        .motion_profile = espui_motion_profile_find("phosphor_default"),
        .density_percent = 100,
        .animation_speed_percent = 100,
    };
    if (espui_ascii_renderer_init(&renderer, &config) != ESP_OK) {
        fprintf(stderr, "failed to initialize ASCII renderer\n");
        return 1;
    }

    if (strcmp(state_name, "all") == 0) {
        for (size_t i = 0; i < sizeof(ALL_STATES) / sizeof(ALL_STATES[0]); ++i) {
            preview_state(&renderer, ALL_STATES[i], frames, delay_ms);
        }
    } else {
        espui_visual_state_t state;
        if (parse_state(state_name, &state) != 0) {
            fprintf(stderr,
                    "usage: %s [all|idle|working|completed|approval|error|offline]"
                    " [frames] [delay_ms]\n",
                    argv[0]);
            espui_ascii_renderer_deinit(&renderer);
            return 2;
        }
        preview_state(&renderer, state, frames, delay_ms);
    }

    espui_ascii_renderer_deinit(&renderer);
    return 0;
}
