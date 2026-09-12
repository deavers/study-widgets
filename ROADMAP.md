# StudyWidgets Roadmap

A living project checklist and long-term memory.
Every agreed feature belongs here before implementation starts.

Legend:

- `[x]` completed
- `[~]` currently in progress
- `[ ]` planned

## v0.1 — Foundation

- [x] Public GitHub repository
- [x] English README, roadmap, .gitignore and MIT license
- [x] CMake project skeleton
- [x] Qt 6.11.2 + MinGW 13.1.0 build environment on Windows
- [x] First Qt application build and launch
- [x] `WidgetBase`: frameless rounded desktop card
- [x] Mouse drag-to-move behavior
- [x] QSettings position persistence
- [x] `WelcomeWidget`: first working widget
- [x] Explicit minimize and application-exit controls
- [x] Plugin-style `WidgetRegistry`
- [~] Basic code cleanup and architecture documentation

## v0.2 — Control Center

- [ ] Control Panel main window
- [ ] Read all widgets from `WidgetRegistry`
- [ ] Enable / disable widgets through checkboxes
- [ ] Save enabled widget state with QSettings
- [ ] System tray icon
- [ ] Restore hidden widgets from the tray
- [ ] Explicit `Exit StudyWidgets` action in tray menu
- [ ] Distinguish widget close from application exit
- [ ] Widget settings button and per-widget configuration dialog
- [ ] Multi-instance widget support

## v0.3 — Data Layer

- [ ] SQLite `DataStore`
- [ ] Database schema versioning and migrations
- [ ] Data backup before migrations
- [ ] Local Markdown activity log: `log.md`
- [ ] Markdown monthly report export
- [ ] JSON import / export for categories and preferences
- [ ] CSV export for study sessions
- [ ] Optional encrypted local database
- [ ] Automatic backup rotation

## v0.4 — Pomodoro and Study Sessions

- [ ] Pomodoro widget: 25 / 50 minute modes
- [ ] Pause / resume
- [ ] Optional 25 / 5 focus-break cycle
- [ ] Editable university subject list
- [ ] `Free study` category
- [ ] Change the active category mid-session
- [ ] Early finish records actual time spent
- [ ] Undo the last recorded session
- [ ] Restore the last undone session
- [ ] Optional short text note after a session
- [ ] Tags: theory, lab, exam, CTF, project, reading
- [ ] Session quality / energy rating, optional and private
- [ ] Per-subject weekly study targets
- [ ] Study-session reminder only when manually enabled

## v0.5 — University Widgets

- [ ] Schedule photo widget
- [ ] Sudá / lichá university week widget
- [ ] Configurable first Monday of the semester
- [ ] Deadlines widget
- [ ] Exam countdown
- [ ] University grade and credit tracker
- [ ] Assignment progress tracker
- [ ] Daily focus widget: one meaningful task only
- [ ] Quick capture: temporary idea / task / note widget

## v0.6 — Habits, Reading and Reflection

- [ ] Habits widget
- [ ] Daily completion toggle
- [ ] Weekly progress: 0–7 days
- [ ] Gentle streak display
- [ ] Streak freeze: one recovery day does not break the chain
- [ ] Avoid failure-focused wording
- [ ] Books widget
- [ ] Current page / total pages
- [ ] Reading progress bar
- [ ] Weekly pages read
- [ ] Evening reflection widget
- [ ] Three prompts: what worked, what was difficult, what matters tomorrow
- [ ] Private local journal export to Markdown

## v0.7 — Statistics and Focus Modes

- [ ] Study statistics widget
- [ ] Day / week / month views
- [ ] Per-subject study-minute graph
- [ ] Weekly comparison
- [ ] GitHub-style yearly study heatmap
- [ ] Subject goal progress rings
- [ ] Game mode: hide all widgets immediately
- [ ] Deep-work mode: 50-minute focus session + hide distractions
- [ ] Fullscreen app detection through Windows APIs
- [ ] Automatic game mode suggestion, never forced
- [ ] Windows toast notifications
- [ ] Global keyboard shortcuts
- [ ] Autostart through Windows Run registry key
- [ ] Light and dark theme
- [ ] Reduced-motion mode

## v0.8 — Quality and Portfolio

- [ ] Unit tests for DataStore
- [ ] Widget Registry tests
- [ ] GitHub Actions CI for Windows
- [ ] GitHub Actions CI for Ubuntu
- [ ] Code formatting configuration
- [ ] Static analysis
- [ ] Versioned releases
- [ ] Windows release packaging with `windeployqt`
- [ ] Installer evaluation: NSIS or Inno Setup
- [ ] Architecture documentation
- [ ] Security and privacy threat model document
- [ ] Dependency inventory / SBOM

## v1.0 — Local Integrations

### Local DataBus

- [ ] Localhost-only HTTP / WebSocket JSON API
- [ ] API authentication token stored locally
- [ ] Clear local API versioning
- [ ] Permission model for connected local apps
- [ ] No externally exposed network listener by default
- [ ] Local audit log for integration events

### bio-tracker

- [ ] Receive local readiness data from bio-tracker
- [ ] Receive pulse and rMSSD / HRV summaries
- [ ] Morning readiness widget
- [ ] Fatigue-aware study suggestion
- [ ] Gentle rest recommendation
- [ ] Eye strain, posture and water reminders
- [ ] Environment signal support: temperature, humidity, room darkness
- [ ] Never present medical diagnosis
- [ ] Keep all biometric data local by default

### deave Second Brain

- [ ] Exchange study sessions with deave
- [ ] Display daily focus from deave
- [ ] Send reflection entries to deave
- [ ] Open relevant deave workspace from a widget
- [ ] Unified local dashboard

### Local AI with Ollama

- [ ] Fully local Ollama connection
- [ ] Daily study summary generated from local data
- [ ] Weekly reflection in a supportive tone
- [ ] Ask-your-progress mini chat
- [ ] Study-plan suggestion based on goals and deadlines
- [ ] Fatigue-aware focus recommendation from bio-tracker data
- [ ] Explicit opt-in before sending any private local data to an LLM
- [ ] No cloud AI requirement

## Design principles

- Local and private by default
- No cloud dependency for core features
- One widget = one focused source file
- No unnecessary background service
- Low CPU, GPU and RAM usage
- Progress over guilt
- Recovery is part of productivity
- Every AI feature is optional
- Every biometric feature is advisory, not medical
- Security boundaries are documented before integrations are added