cmake_minimum_required(VERSION 3.31)
project(Graphics)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Network
        REQUIRED)

add_executable(Graphics main.cpp
        SquareWidget.cpp
        SquareWidget.h)
target_link_libraries(Graphics
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt6::Network
)

