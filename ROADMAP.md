# StudyWidgets Roadmap

A living project checklist and long-term memory.

Every agreed feature belongs here before implementation starts.

Legend:

- `[x]` completed
- `[~]` currently in progress
- `[ ]` planned

## v0.1 — Foundation

- [x] Public GitHub repository
- [x] English README, roadmap, `.gitignore` and MIT license
- [x] CMake project skeleton
- [x] Qt 6.11.2 + MinGW 13.1.0 build environment on Windows
- [x] First Qt application build and launch
- [x] `WidgetBase`: frameless rounded desktop card
- [x] Mouse drag-to-move behavior
- [x] QSettings widget-position persistence
- [x] `WelcomeWidget`: first working widget
- [x] Plugin-style `WidgetRegistry`
- [x] Registry-based widget creation
- [x] Basic project architecture documentation

## v0.2 — Control Center and Tray

- [x] Control Panel main window
- [x] Read registered widgets from `WidgetRegistry`
- [x] Enable / disable widgets through checkboxes
- [x] Save widget enabled state with QSettings
- [x] Windows System Tray icon
- [x] Tray context menu
- [x] Restore widgets from tray
- [x] Hide all active widgets
- [x] Close Control Panel to tray
- [x] Explicit `Exit StudyWidgets` tray action
- [x] Widget hide-to-tray callback
- [x] Basic Game Mode
- [x] Game Mode button state: ON / OFF
- [x] Synchronized Game Mode tray checkbox
- [ ] Start minimized to tray
- [ ] Per-widget settings button
- [ ] Multi-instance widget support
- [ ] User-selectable widget ordering
- [ ] Save and restore named desktop layouts
- [ ] Profiles: Study / Deep Work / Gaming / Minimal
- [ ] Quiet hours and notification do-not-disturb mode

## v0.3 — Local Data Layer

- [ ] SQLite `DataStore`
- [ ] Database schema versioning
- [ ] Safe migrations
- [ ] Automatic database backup before migrations
- [ ] Local Markdown activity feed: `log.md`
- [ ] Monthly Markdown report export
- [ ] JSON export / import for settings and categories
- [ ] CSV export for study sessions
- [ ] Optional encrypted local database
- [ ] Backup rotation
- [ ] Data health check and corruption-safe recovery
- [ ] Session audit trail for manual edits and undo operations

## v0.4 — Pomodoro and Study Sessions

- [ ] Pomodoro widget: 25 / 50 minute modes
- [ ] Pause / resume
- [ ] Optional 25 / 5 focus-break cycle
- [ ] Editable university subject list
- [ ] `Free study` category
- [ ] Change active category mid-session
- [ ] Early finish records actual time spent
- [ ] Undo the last recorded session
- [ ] Restore the last undone session
- [ ] Optional short note after a session
- [ ] Tags: theory, lab, exam, CTF, project, reading
- [ ] Optional energy / concentration rating
- [ ] Per-subject weekly study targets
- [ ] Pomodoro completion notification
- [ ] Optional sound and vibration-style notification profiles
- [ ] Session interruption reason: break, task complete, fatigue, distraction
- [ ] Focus-session history

## v0.5 — University Widgets

- [ ] Schedule photo widget
- [ ] Sudá / lichá university week parity widget
- [ ] Configurable first Monday of semester
- [ ] Deadlines widget
- [ ] Exam countdown
- [ ] Grade and credit tracker
- [ ] Assignment progress tracker
- [ ] Daily focus widget: one meaningful task only
- [ ] Quick capture: temporary task / idea / note widget
- [ ] Semester goal progress
- [ ] Exam revision planner

## v0.6 — Habits, Reading and Reflection

- [ ] Habits widget
- [ ] Daily completion toggle
- [ ] Weekly progress: 0–7 days
- [ ] Gentle streak display
- [ ] Streak freeze: one recovery day does not break the chain
- [ ] Recovery-friendly wording instead of failure-focused wording
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
- [ ] Subject-goal progress rings
- [ ] Fullscreen app detection through Windows APIs
- [ ] Automatic Game Mode suggestion, never forced
- [ ] Windows toast notifications
- [ ] Global keyboard shortcuts
- [ ] Autostart through Windows Run registry key
- [ ] Light and dark themes
- [ ] Reduced-motion mode
- [ ] High-contrast accessibility option
- [ ] Configurable font scale
- [ ] Per-widget opacity and click-through mode

## v0.8 — Quality and Portfolio

- [ ] Unit tests for `DataStore`
- [ ] Widget Registry tests
- [ ] GitHub Actions CI for Windows
- [ ] GitHub Actions CI for Ubuntu
- [ ] Code formatting configuration
- [ ] Static analysis
- [ ] Versioned releases
- [ ] Windows release packaging with `windeployqt`
- [ ] Installer evaluation: NSIS or Inno Setup
- [ ] Architecture documentation
- [ ] Security and privacy threat-model document
- [ ] Dependency inventory / SBOM
- [ ] User-facing privacy documentation
- [ ] Crash log opt-in, stored locally

## v1.0 — Local Integrations

### Local DataBus

- [ ] Localhost-only HTTP / WebSocket JSON API
- [ ] API versioning
- [ ] Local API authentication token
- [ ] Explicit permission model for connected local apps
- [ ] No externally exposed network listener by default
- [ ] Local audit log for integration events
- [ ] Rate limiting for local API calls
- [ ] Secure secret storage for API tokens

### bio-tracker

- [ ] Receive local readiness data from bio-tracker
- [ ] Receive pulse and rMSSD / HRV summaries
- [ ] Morning readiness widget
- [ ] Fatigue-aware study suggestion
- [ ] Gentle rest recommendation
- [ ] Eye strain, posture and water reminders
- [ ] Environment signal support: temperature, humidity, room darkness
- [ ] Never present medical diagnosis
- [ ] Keep biometric data local by default
- [ ] Configurable baseline windows for HRV / resting pulse
- [ ] Quiet fatigue alerts during fullscreen games
- [ ] Queue non-urgent wellbeing suggestions until focus mode ends

### deave Second Brain

- [ ] Exchange study sessions with deave
- [ ] Display daily focus from deave
- [ ] Send reflection entries to deave
- [ ] Open relevant deave workspace from a widget
- [ ] Unified local dashboard
- [ ] Shared local search index

### Local AI with Ollama

- [ ] Fully local Ollama connection
- [ ] Daily study summary generated from local data
- [ ] Weekly reflection in a supportive tone
- [ ] Ask-your-progress mini chat
- [ ] Study-plan suggestion based on goals and deadlines
- [ ] Fatigue-aware focus suggestion from bio-tracker data
- [ ] Explicit opt-in before sending any private local data to an LLM
- [ ] No cloud AI requirement
- [ ] Show the exact local data sent to an LLM before generation
- [ ] Configurable local model and context limits

## Design Principles

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