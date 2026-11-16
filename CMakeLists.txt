cmake_minimum_required(VERSION 3.16)

project(StudyWidgets VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Multimedia Network)

add_executable(StudyWidgets WIN32
    src/main.cpp
    src/Theme.h
    src/Theme.cpp
    src/AlarmPlayer.h
    src/AlarmPlayer.cpp
    src/GlobalHotkey.h
    src/GlobalHotkey.cpp
    src/TimeLogWriter.h
    src/TimeLogWriter.cpp
    src/LegacyImporter.h
    src/LegacyImporter.cpp
    src/WidgetBase.h
    src/WidgetRegistry.h
    src/PrayerCalculator.h
    src/DataStore.h
    src/DataStore.cpp
    src/api/DeaveApiClient.h
    src/api/DeaveApiClient.cpp
    src/CategoryConfigLoader.h
    src/CategoryConfigLoader.cpp
    src/ControlPanel.h
    src/ControlPanel.cpp
    src/ui/GlassWindow.h
    src/ui/GlassWindow.cpp
    src/ui/WidgetChrome.h
    src/ui/FlowLayout.h
    src/ui/FlowLayout.cpp
    src/ui/SubjectTile.h
    src/ui/SubjectTile.cpp
    src/widgets/PomodoroWidget.h
    src/widgets/PomodoroWidget.cpp
    src/widgets/ScheduleWidget.h
    src/widgets/ScheduleWidget.cpp
    src/widgets/PrayerTimesWidget.h
    src/widgets/PrayerTimesWidget.cpp
    src/widgets/SpacedRepetitionWidget.h
    src/widgets/SpacedRepetitionWidget.cpp
    src/widgets/SystemMonitorWidget.h
    src/widgets/SystemMonitorWidget.cpp
    src/widgets/DailyWisdomWidget.h
    src/widgets/DailyWisdomWidget.cpp
    src/widgets/GoalsWidget.h
    src/widgets/GoalsWidget.cpp
    src/widgets/RoadmapWidget.h
    src/widgets/RoadmapWidget.cpp
    src/widgets/CalendarWidget.h
    src/widgets/CalendarWidget.cpp
)

target_include_directories(StudyWidgets PRIVATE src)

target_link_libraries(
    StudyWidgets
    PRIVATE
        Qt6::Widgets
        Qt6::Sql
        Qt6::Multimedia
        Qt6::Network
        dwmapi
        iphlpapi
)
