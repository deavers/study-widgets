# StudyWidgets

A local-first desktop study companion built with C++ and Qt.

StudyWidgets will provide lightweight desktop widgets for focused study:
Pomodoro sessions, subject tracking, habits, books, schedules, university
week parity, statistics, and future biometric / local-AI integrations.

> Status: active early development. The application currently includes a
> working Qt + CMake + MinGW environment, frameless desktop widgets,
> a plugin-style widget registry, a Control Panel and Windows system tray support.

## Current milestone

The current version provides the first working application shell:

- Frameless, rounded desktop-style `WelcomeWidget`
- Mouse drag-to-move behavior
- Widget position persistence through `QSettings`
- Plugin-style `WidgetRegistry`
- Registry-driven Control Panel
- Enable / disable widget checkboxes
- Windows System Tray icon and menu
- Hide widgets to tray
- Restore widgets from tray
- Explicit `Exit StudyWidgets` tray action
- Game Mode with synchronized UI state:
  - `Game mode: OFF`
  - `Game mode: ON`
  - checked tray menu action
- Widgets remain lightweight while hidden
- Local SQLite DataStore foundation
- Database schema versioning
- Local `study_sessions` table ready for Pomodoro data
- Automatic database creation through QStandardPaths

## Features

| Area | Planned capabilities |
|---|---|
| Pomodoro | 25/50 minute sessions, pauses, subjects, free-study category, early finish tracking, undo/restore |
| Study tracking | Per-subject minutes, daily / weekly / monthly statistics, targets and progress |
| Habits | Daily check-offs, gentle streaks, weekly progress and recovery days |
| Books | Reading list, pages read, progress bars and weekly reading count |
| University | Schedule photo, Sudá / lichá week parity, deadlines, exams and credits |
| Focus | Daily focus item, deep-work mode and anti-overwhelm workflow |
| Modes | Game mode, tray support, autostart, global hotkeys and notifications |
| Integrations | bio-tracker readiness data, deave Second Brain and local Ollama assistant |

## Principles

- **Local-first** — no cloud account, telemetry or forced online service
- **Private by default** — study data stays on the user's device
- **Lightweight** — widgets should be nearly idle while hidden or inactive
- **Modular** — one widget is one focused C++ source file
- **Gentle productivity** — progress, context and recovery instead of guilt
- **Integration-ready** — future local communication through localhost APIs
- **Security-conscious** — integrations remain opt-in, local and documented

## Architecture

```text
src/
├── main.cpp
├── WidgetBase.h
├── WidgetRegistry.h
├── DataStore.h
├── DataStore.cpp
├── ControlPanel.h
├── ControlPanel.cpp
└── widgets/
    ├── WelcomeWidget.h
    └── WelcomeWidget.cpp
```

### WidgetBase

`WidgetBase` is the common base class for desktop widgets.

It provides:

- Frameless window behavior
- Rounded custom widget background
- Drag-to-move behavior
- Saved window position through QSettings
- Always-on-top widget behavior
- Generic `hideToTray()` callback support

### WidgetRegistry

Widgets self-register through a small macro:

```cpp
REGISTER_WIDGET(
    "welcome",
    "Welcome widget",
    WelcomeWidget
)
```

The Control Panel reads the Registry and automatically creates a checkbox for
each registered widget. Later, adding a new widget will require only:

1. Add one `.cpp` / `.h` widget implementation
2. Register it with `REGISTER_WIDGET`
3. Rebuild the application

### Control Panel

The Control Panel is the central application controller.

It owns:

- Widget enable/disable state
- Active widget instances
- System Tray icon
- Tray context menu
- Game Mode state
- Widget restoration after hiding

### System Tray

StudyWidgets stays available in the Windows System Tray.

Tray actions:

```text
Open Control Panel
Show widgets
Hide widgets
Game mode
Exit StudyWidgets
```

Closing the Control Panel hides it to tray rather than terminating the app.

### Local DataStore

`DataStore` manages the application's local SQLite database.

Current responsibilities:

- Open the local database during application startup
- Create the first database schema automatically
- Track the database schema version
- Create the `study_sessions` table
- Add a study session
- Read recent study sessions
- Return readable local database errors

The database is currently stored in the Windows application-data directory:

```text
%APPDATA%\deavers\StudyWidgets\studywidgets.sqlite
```

No study data is sent to a cloud service.

## Build on Windows

### Requirements

- Qt 6.11.2 with `mingw_64`
- MinGW 13.1.0 from Qt Tools
- CMake
- Git
- PowerShell

### Configure the current PowerShell session

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.2\mingw_64\bin;$env:Path"
```

### Configure, build and run

```powershell
cmake -S . -B build `
  -G "MinGW Makefiles" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.2/mingw_64" `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel

.\build\StudyWidgets.exe
```

## Project roadmap

See [ROADMAP.md](ROADMAP.md) for the full development plan.

## License

This project is licensed under the [MIT License](LICENSE).