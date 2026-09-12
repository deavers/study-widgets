# StudyWidgets

A local-first desktop study companion built with C++ and Qt.

StudyWidgets will provide lightweight desktop widgets for focused study:
Pomodoro sessions, subject tracking, habits, books, schedules, statistics,
semester week parity, and future biometric / local-AI integrations.

> Status: active early development. The project currently has a working
> CMake + Qt6 + MinGW setup, a draggable frameless base widget, and a
> plugin-style widget registry.

## Preview

Current milestone:

- Frameless desktop-style Welcome widget
- Rounded dark UI card
- Mouse dragging
- Position persistence through QSettings
- Minimize and explicit application exit controls
- Plugin-style widget registration

## Features

| Area | Planned capabilities |
|---|---|
| Pomodoro | 25/50 minute sessions, pauses, subjects, free-study category, early finish tracking, undo/restore |
| Study tracking | Per-subject minutes, daily / weekly / monthly statistics, targets and progress |
| Habits | Daily check-offs, gentle streaks, weekly progress and recovery days |
| Books | Reading list, pages read, progress bars and weekly reading count |
| University | Schedule photo, Sudá / lichá week parity, deadlines, exams and credits |
| Focus | Daily focus item, deep-work mode, anti-overwhelm workflow |
| Modes | Game mode, system tray, autostart, global hotkeys and notifications |
| Integrations | bio-tracker readiness data, deave Second Brain, local Ollama assistant |

## Principles

- **Local-first** — no cloud account, no telemetry and no forced online service.
- **Private by default** — study data stays on the user's machine.
- **Lightweight** — widgets should be nearly idle when not actively used.
- **Modular** — one widget is one focused C++ source file.
- **Gentle productivity** — show progress, context and recovery instead of guilt.
- **Integration-ready** — communicate with bio-tracker, deave and local AI through localhost APIs.

## Current architecture

```text
src/
├── main.cpp
├── WidgetBase.h
├── WidgetRegistry.h
└── widgets/
    ├── WelcomeWidget.h
    └── WelcomeWidget.cpp
```

### WidgetBase

`WidgetBase` is the foundation for every desktop widget:

- Frameless window
- Rounded custom background
- Drag-to-move behavior
- Saved position through QSettings
- Always-on-top desktop widget behavior

### WidgetRegistry

Widgets register themselves using:

```cpp
REGISTER_WIDGET(
    "welcome",
    "Welcome widget",
    WelcomeWidget
)
```

Later, the Control Panel will read this registry and allow the user to
enable, disable and configure every available widget without changing
`main.cpp`.

## Build on Windows

### Requirements

- Qt 6.11.2 with `mingw_64`
- MinGW 13.1.0 from Qt Tools
- CMake
- Git
- PowerShell

### Configure the current shell

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

The complete development plan is maintained in [ROADMAP.md](ROADMAP.md).

## License

This project is licensed under the [MIT License](LICENSE).