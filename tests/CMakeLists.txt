# =============================================================================
#  VideoForge 单元测试
#  只链接 videoforge_core（纯逻辑层），不需要界面、不需要真实视频文件，
#  因此可以在任何环境、任何 CI 上秒级跑完。
# =============================================================================

add_executable(vf_tests
    test_core.cpp
)

target_link_libraries(vf_tests PRIVATE videoforge_core)

add_test(NAME vf_tests COMMAND vf_tests)

# 端到端测试：需要真实 ffmpeg + 真实视频，单独一个脚本，默认手动执行
add_test(
    NAME vf_e2e
    COMMAND ${CMAKE_COMMAND}
        -DVIDEOFORGE_EXE=$<TARGET_FILE:VideoForge>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/e2e
        -P ${CMAKE_CURRENT_SOURCE_DIR}/e2e_convert.cmake
)
set_tests_properties(vf_e2e PROPERTIES DISABLED TRUE)
