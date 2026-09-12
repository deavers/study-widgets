cmake_minimum_required(VERSION 3.16)

project(StudyWidgets VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

add_executable(StudyWidgets
    src/main.cpp
    src/WidgetBase.h
    src/WidgetRegistry.h
    src/DataStore.h
    src/DataStore.cpp
    src/CategoryConfigLoader.h
    src/CategoryConfigLoader.cpp
    src/ControlPanel.h
    src/ControlPanel.cpp
    src/widgets/WelcomeWidget.h
    src/widgets/WelcomeWidget.cpp
    src/widgets/PomodoroWidget.h
    src/widgets/PomodoroWidget.cpp
)

target_include_directories(StudyWidgets PRIVATE src)

target_link_libraries(
    StudyWidgets
    PRIVATE
        Qt6::Widgets
        Qt6::Sql
)