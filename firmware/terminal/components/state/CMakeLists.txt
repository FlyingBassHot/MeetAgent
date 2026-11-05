idf_component_register(
    SRCS
        "state_adapter.c"
        "mock_state.c"
    INCLUDE_DIRS "include"
    REQUIRES
        espui
        protocol
)
