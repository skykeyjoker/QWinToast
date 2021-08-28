cmake_minimum_required (VERSION 3.8)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

project ("QWinToastExample")

find_package(Qt5 COMPONENTS Gui Widgets)

add_executable(QWinToastExample main.cpp ../Src/QWinToast.h ../Src/QWinToast.cpp QWinToastExample.h QWinToastExample.cpp)

target_link_libraries(QWinToastExample Qt5::Widgets Qt5::Gui)

