cmake_minimum_required(VERSION 3.20)
project(ActivityTicketing LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT WIN32)
  message(FATAL_ERROR "本程序使用 Win32 和 DirectX 11，只能在 Windows 上构建")
endif()

# 业务核心：领域模型、数据层和 12 个业务模块，不依赖界面。
add_library(ticketing_core STATIC
  src/common/file_util.cc
  src/common/time_util.cc
  src/domain/model.cc
  src/data/store.cc
  src/service/account_service.cc
  src/service/activity_service.cc
  src/service/audit_log.cc
  src/service/checkin_service.cc
  src/service/comment_service.cc
  src/service/favorite_service.cc
  src/service/notifier.cc
  src/service/query_service.cc
  src/service/registration_service.cc
  src/service/review_service.cc
  src/service/scheduler.cc
  src/service/stats_service.cc
  src/service/system.cc
)
target_include_directories(ticketing_core PUBLIC src)
target_compile_options(ticketing_core PRIVATE -Wall -Wextra)

# 第三方界面库 Dear ImGui（Win32 + DirectX 11 后端），不计入本项目代码量。
set(IMGUI_DIR third_party/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
  ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# 桌面程序。
add_executable(ticketing_app WIN32
  src/main.cc
  src/ui/app.cc
  src/ui/screens_public.cc
  src/ui/screens_personal.cc
  src/ui/screens_manage.cc
  src/ui/theme.cc
  src/ui/texture.cc
)
target_include_directories(ticketing_app PRIVATE src)
# 第三方头文件按系统头文件处理，不显示其中的编译警告。
target_include_directories(ticketing_app SYSTEM PRIVATE third_party)
target_compile_options(ticketing_app PRIVATE -Wall -Wextra)
target_link_libraries(ticketing_app PRIVATE ticketing_core imgui
  d3d11 d3dcompiler dxgi dwmapi gdi32 imm32)
# 静态链接运行库，exe 拷到其他电脑也能直接运行。
target_link_options(ticketing_app PRIVATE -static)

# 程序运行时需要的图片资源，复制到 exe 旁边。
add_custom_command(TARGET ticketing_app POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/assets/mascot.png $<TARGET_FILE_DIR:ticketing_app>/mascot.png)

# 业务规则测试。
enable_testing()
add_executable(ticketing_tests tests/core_test.cc)
target_link_libraries(ticketing_tests PRIVATE ticketing_core)
target_compile_options(ticketing_tests PRIVATE -Wall -Wextra)
target_link_options(ticketing_tests PRIVATE -static)
add_test(NAME core_test COMMAND ticketing_tests)
