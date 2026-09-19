cmake_minimum_required(VERSION 3.20)
project(ExcelSearch VERSION 0.3.2 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# .rc 里的 app.ico / app_qt.manifest 用相对名引用，windres 需要源码目录在搜索路径里
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -I \"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Qt6（MinGW：C:/msys64/mingw64）；Network 用于单实例闸门（QLocalServer/QLocalSocket）
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)   # app_icon.qrc：把 app.ico 嵌进 Qt 资源，供托盘/任务栏统一取用

# —— core 与 thirdparty 一律走本仓库相对路径 ——
#   原型阶段引用的是绝对路径 <repo-v0.3.0>；回迁后必须自包含（可独立构建/公开）。
set(TP        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
set(MINIZ     ${TP})
set(PUGI      ${TP})
set(LIBXLS    ${TP}/libxls)
set(WINICONV  ${TP}/win_iconv)
set(RAPIDFUZZ ${TP}/rapidfuzz)

add_executable(excel_search
    main.cpp
theme.cpp
    app_icon.qrc         # 内嵌 app.ico（托盘/任务栏/资源管理器 三处图标统一）
    manual.qrc           # 内嵌 MANUAL.md（外部文件缺失时的兜底）
    app_qt.rc            # 图标 + 清单(asInvoker/PerMonitorV2) + 版本信息(V0.3.2)
    search_engine.cpp
    xlsx_reader.cpp
    xls_reader.cpp
    csv_reader.cpp
    docx_reader.cpp
    pinyin.cpp
    core/file_io.cpp
    xse_codec.cpp
    crypto_win.cpp
    # 注：ocr_reader.cpp 按既定版本归属暂不纳入本版构建（见 docs/回迁标注.md 第 16 条）；
    #     源文件保留在目录内，勿删。
    ${MINIZ}/miniz.c
    ${MINIZ}/miniz_tdef.c
    ${MINIZ}/miniz_tinfl.c
    ${MINIZ}/miniz_zip.c
    ${PUGI}/pugixml.cpp
    ${LIBXLS}/src/endian.c
    ${LIBXLS}/src/locale.c
    ${LIBXLS}/src/ole.c
    ${LIBXLS}/src/xls.c
    ${LIBXLS}/src/xlstool.c
    ${WINICONV}/win_iconv.c
)

# ★ GUI 子系统（MinGW 等价 -mwindows，MSVC 等价 /SUBSYSTEM:WINDOWS）：
#   漏设会链接出控制台程序 → 双击运行时弹黑色命令行窗口（详见 docs/回迁标注.md 第 17 条）。
#   附带影响：GUI 子系统不分配控制台，自检钩子写文件（--out），不往 stdout 打印。
# ---- 敏感口令：构建期注入（仓库内不保存真值）----
#   真值放在本机 local_secrets.cmake（已在 .gitignore 中，不随仓库发布）；
#   照 local_secrets.cmake.example 复制一份并填值即可。未提供时用代码内的安全默认（超管通道关闭）。
include("${CMAKE_CURRENT_SOURCE_DIR}/local_secrets.cmake" OPTIONAL)
target_compile_definitions(excel_search PRIVATE
    "ES_SUPER_PASSWORD=\"${ES_SUPER_PASSWORD}\""
    "ES_DEFAULT_ADMIN_PASSWORD=\"${ES_DEFAULT_ADMIN_PASSWORD}\"")
set_target_properties(excel_search PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(excel_search PRIVATE Qt6::Widgets Qt6::Network bcrypt)

target_include_directories(excel_search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MINIZ} ${PUGI}
    ${LIBXLS}/include ${LIBXLS}/src ${WINICONV} ${RAPIDFUZZ}
)

if(MINGW)
    target_compile_options(excel_search PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()
