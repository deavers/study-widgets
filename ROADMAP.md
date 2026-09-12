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
- [x] One-action application exit closes all active widgets
- [x] Exit lifecycle handler shared by desktop widgets
- [x] Save active Pomodoro session during app exit
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

- [x] SQLite `DataStore` foundation
- [x] Automatic local SQLite database creation
- [x] Database schema versioning foundation
- [x] Schema version 1
- [x] `study_sessions` table
- [x] Add study-session API
- [x] Read recent study-session API
- [x] Human-readable SQLite error reporting
- [x] Daily study-duration query API
- [x] Exact daily study total calculation
- [ ] Safe migrations for future schema versions
- [ ] Automatic database backup before migrations
- [ ] Local Markdown activity feed: `log.md`
- [ ] Monthly Markdown report export
- [ ] JSON export / import for settings and categories
- [ ] CSV export for study sessions
- [ ] Optional encrypted local database
- [ ] Backup rotation
- [ ] Data health check and corruption-safe recovery
- [ ] Session audit trail for manual edits and undo operations
- [ ] User-selectable data directory

## v0.4 — Pomodoro and Study Sessions

- [x] Pomodoro widget foundation
- [x] 25 / 50 minute focus modes
- [x] Start / pause / resume
- [x] Accurate one-second countdown
- [x] `Qt::PreciseTimer` countdown updates
- [x] Default study categories
- [x] `Free study` category
- [x] Change active category mid-session
- [x] Early finish records actual time spent
- [x] Completed sessions record `completed = true`
- [x] Early sessions record `completed = false`
- [x] Save active Pomodoro session during application exit
- [x] Local SQLite session persistence
- [x] Daily duration summary from SQLite
- [x] SQLite-backed category IDs
- [x] Category persistence independent from visible names
- [x] Private local JSON category configuration
- [x] CategoryConfigLoader JSON validation
- [x] Local category synchronization into SQLite
- [x] Archive categories removed from local configuration
- [x] Pomodoro categories loaded from SQLite
- [x] Future-ready `selectCategoryById()` API for Subject Board
- [ ] Editable subject/category management interface
- [ ] Shared selected-category state between widgets
- [ ] Subject Board click selects a Pomodoro category
- [ ] Subject Board restores hidden Pomodoro widget
- [ ] Subject Board never auto-starts a Pomodoro timer
- [ ] Pending category selection for the next session
- [ ] Safe behavior when Pomodoro is already running
- [ ] Custom category colors and icons
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
- [ ] Automatic short-break widget
- [ ] Long break after configurable number of sessions

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
- [ ] Compact Subject Board widget
- [ ] Clickable subject rows
- [ ] Today / week / month subject views
- [ ] Subject Board → Pomodoro handoff
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

## v0.9 — Privacy and Security

- [x] Local-first project principle
- [x] No telemetry / no cloud requirement for core features
- [x] Private-data exclusions in `.gitignore`
- [x] Public `SECURITY.md` policy
- [x] Parameterized SQLite queries
- [x] Input validation before study-session writes
- [x] SQLite foreign-key enforcement
- [x] Private local category configuration outside Git repository
- [x] Generic public category example file
- [x] Local JSON schema validation for category configuration
- [x] No real university subjects required in public source code
- [ ] Threat model document using STRIDE or similar methodology
- [ ] Local data classification: public / private / sensitive
- [ ] Data-retention and deletion policy
- [ ] Automatic encrypted local backup option
- [ ] SQLCipher evaluation for optional encrypted SQLite storage
- [ ] Windows DPAPI integration for local API tokens
- [ ] Windows Credential Manager integration
- [ ] GitHub secret scanning and pre-commit secret checks
- [ ] Dependency vulnerability scanning
- [ ] SBOM generation for releases
- [ ] Security-focused code review checklist
- [ ] Local log redaction
- [ ] Permission dashboard for integrations
- [ ] Privacy dashboard: show stored data locations

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