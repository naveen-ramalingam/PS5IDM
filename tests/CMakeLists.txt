# ═══════════════════════════════════════════════════════════════════════════════
# PS5 Download Manager - Tests
# ═══════════════════════════════════════════════════════════════════════════════

# For now, create a placeholder test
add_executable(ps5dm_test_basic
    test_basic.cpp
)
target_link_libraries(ps5dm_test_basic PRIVATE ps5dm_lib)
add_test(NAME BasicTest COMMAND ps5dm_test_basic)
