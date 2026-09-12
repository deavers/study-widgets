\# StudyWidgets 🎛



Local-first desktop study widgets: a pomodoro timer with subject tracking,

habits, books, schedule and weekly stats. Built with \*\*C++ / Qt\*\*.

No cloud, no accounts, no telemetry — all data stays on your machine.



> 🚧 Status: early development (v0.1). See \[ROADMAP.md](ROADMAP.md) for the full plan.



\## Features



| Widget | Description |

|---|---|

| 🍅 Pomodoro | 25/50 min modes, subject selection or "Free swim" category, switch category mid-session, early finish still counts, undo/restore last entry |

| 📊 Stats | Study minutes per subject for day / week / month |

| ✅ Habits | Daily check-off, 🔥 streak, weekly progress |

| 📚 Books | Pages read, progress bars, weekly page count |

| 🗓 Schedule | Your timetable photo pinned to the desktop |

| 📅 Week parity | Sudá (even) / lichá (odd) university week |

| 🎮 Game mode | Hide all widgets with zero CPU/GPU cost |



\## Principles



\- \*\*Local \& private\*\* — SQLite database on your disk, nothing leaves your machine

\- \*\*One widget = one .cpp\*\* — drop a file into `src/widgets/`, rebuild, done

\- \*\*Lightweight\*\* — a sleeping desktop companion, not a background service

\- \*\*Gentle framing\*\* — progress over guilt; a missed day doesn't break the streak

\- \*\*Integration-ready\*\* — designed to connect to local AI (Ollama) and

&#x20; biometric trackers (pulse, HRV) through a local JSON API



\## Build



\### Windows



Prerequisites: \[Qt online installer](https://www.qt.io/download-open-source)

(Qt 6.x, MinGW or MSVC) and CMake.



```powershell

cmake -B build -DCMAKE\_PREFIX\_PATH="C:/Qt/6.9.1/mingw\_64" -DCMAKE\_BUILD\_TYPE=Release

cmake --build build

.\\build\\StudyWidgets.exe

```



\### Linux



```bash

sudo apt install build-essential cmake qt6-base-dev libqt6sql6-sqlite

cmake -B build -DCMAKE\_BUILD\_TYPE=Release \&\& cmake --build build

./build/StudyWidgets

```



\## Project structure



```text

src/

├── main.cpp              # entry point

├── WidgetRegistry.h      # plugin-style widget registration

├── WidgetBase.h          # frameless draggable card widget

├── ControlPanel.h/.cpp   # enable/disable widgets, game mode, autostart

├── DataStore.h/.cpp      # SQLite + Markdown persistence

└── widgets/              # one .cpp per widget — add yours here

```



\## Adding your own widget



1\. Copy `src/widgets/ExampleWidget.cpp` to `MyWidget.cpp`

2\. Change the id, title and the UI inside

3\. `cmake --build build` — your widget appears in the control panel automatically



\## Roadmap



See \[ROADMAP.md](ROADMAP.md) — from the widget core to local-AI summaries

and bio-tracker fatigue alerts.



\## License



\[MIT](LICENSE)

