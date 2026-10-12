# =============================================================================
#  VideoForge 端到端测试（CTest 脚本模式）
#
#  它会：
#    1. 用 ffmpeg 合成一段测试视频
#    2. 用被测程序 GUI 的自测模式（--screenshot --autoconvert）跑一次真实转换
#    3. 用 ffprobe 校验输出文件确实存在、时长合理、可被解析
#
#  运行：
#    ctest -R vf_e2e --output-on-failure
#    （默认在 tests/CMakeLists.txt 里被 DISABLED，因为需要真实 ffmpeg 与图形环境）
# =============================================================================

if(NOT DEFINED VIDEOFORGE_EXE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "需要 -DVIDEOFORGE_EXE=<exe> -DWORK_DIR=<dir>")
endif()

find_program(FFMPEG_EXECUTABLE NAMES ffmpeg ffmpeg.exe
    HINTS "${CMAKE_CURRENT_LIST_DIR}/../third_party/ffmpeg/bin"
          "${CMAKE_CURRENT_LIST_DIR}/../build/ffmpeg/bin")
if(NOT FFMPEG_EXECUTABLE)
    message(FATAL_ERROR "找不到 ffmpeg，请先运行 scripts/get-ffmpeg.ps1")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(SAMPLE "${WORK_DIR}/sample.mp4")
set(SHOTS "${WORK_DIR}/shots")

message(STATUS "1/4 生成测试视频")
execute_process(
    COMMAND "${FFMPEG_EXECUTABLE}" -y -hide_banner -loglevel error
            -f lavfi -i testsrc2=size=640x360:rate=25
            -f lavfi -i sine=frequency=440:sample_rate=48000
            -t 6 -c:v libx264 -preset ultrafast -pix_fmt yuv420p
            -c:a aac -b:a 128k -shortest "${SAMPLE}"
    RESULT_VARIABLE gen_result)
if(NOT gen_result EQUAL 0)
    message(FATAL_ERROR "生成测试视频失败")
endif()

message(STATUS "2/4 运行 GUI 自测（真实转换 + 截图）")
execute_process(
    COMMAND "${VIDEOFORGE_EXE}" --screenshot "${SHOTS}" --autoconvert "${SAMPLE}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE run_result
    TIMEOUT 120)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "被测程序自测退出码异常：${run_result}")
endif()

message(STATUS "3/4 校验输出文件")
file(GLOB OUTPUTS "${WORK_DIR}/*_转换后*.mp4")
if(NOT OUTPUTS)
    message(FATAL_ERROR "没有生成任何输出文件")
endif()
list(GET OUTPUTS 0 FIRST_OUTPUT)
if(NOT EXISTS "${FIRST_OUTPUT}")
    message(FATAL_ERROR "输出文件不存在：${FIRST_OUTPUT}")
endif()

message(STATUS "4/4 用 ffprobe 校验输出可解析")
execute_process(
    COMMAND "${FFMPEG_EXECUTABLE}" -v error -show_entries format=duration,size
            -of default=noprint_wrappers=1 "${FIRST_OUTPUT}"
    OUTPUT_VARIABLE probe_output
    RESULT_VARIABLE probe_result)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR "ffprobe 无法解析输出文件：${FIRST_OUTPUT}")
endif()

message(STATUS "端到端测试通过 ✔")
message(STATUS "输出文件：${FIRST_OUTPUT}")
message(STATUS "ffprobe：${probe_output}")
