# =============================================================================
#  VideoForge 单元测试
#
#  vf_tests       : core 层纯逻辑（不需要 GUI、不需要 ffmpeg）
#  vf_tests_job   : job 层状态机与队列（用 tests/fake_ffmpeg.cpp 当测试替身）
#  vf_fake_ffmpeg : 测试替身本身，仅供上面两个测试调用
# =============================================================================

# ---- 测试替身：可切换行为的"假 ffmpeg" ----
add_executable(vf_fake_ffmpeg fake_ffmpeg.cpp)
target_link_libraries(vf_fake_ffmpeg PRIVATE Qt6::Core)

# ---- core 层测试 ----
add_executable(vf_tests test_core.cpp)
target_link_libraries(vf_tests PRIVATE videoforge_core)
add_test(NAME vf_tests COMMAND vf_tests)

# ---- job 层测试 ----
add_executable(vf_tests_job test_job.cpp)
target_link_libraries(vf_tests_job PRIVATE videoforge_core)
add_dependencies(vf_tests_job vf_fake_ffmpeg)
add_test(NAME vf_tests_job COMMAND vf_tests_job)

# ---- 端到端测试：需要真实 ffmpeg + 真实视频，默认手动执行 ----
add_test(
    NAME vf_e2e
    COMMAND ${CMAKE_COMMAND}
        -DVIDEOFORGE_EXE=$<TARGET_FILE:VideoForge>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/e2e
        -P ${CMAKE_CURRENT_SOURCE_DIR}/e2e_convert.cmake
)
set_tests_properties(vf_e2e PROPERTIES DISABLED TRUE)
