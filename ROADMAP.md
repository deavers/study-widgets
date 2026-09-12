# 🗺 StudyWidgets Roadmap

A living checklist. This file is the project's long-term memory —
every agreed idea lands here so nothing is lost.

## v0.1 — Core
- [ ] Project skeleton: CMake + Qt6 + empty window (Windows: Qt online installer, MinGW)
- [ ] Widget registry (REGISTER_WIDGET) — a new widget is one .cpp in `src/widgets/`
- [ ] BaseWidget: frameless card, drag to move, position saved per widget
- [ ] Control panel: enable/disable checkboxes
- [ ] README, .gitignore, LICENSE

## v0.2 — Data
- [ ] DataStore: SQLite (sessions, habits, books, journal)
- [ ] log.md feed — every record mirrored to Markdown
- [ ] DB schema versioning (`schema_version` + migrations)
- [ ] report.md export

## v0.3 — Pomodoro
- [ ] 25/50 min, pause/resume
- [ ] Categories: subjects + "🌊 Free swim", editable list
- [ ] Change category mid-session
- [ ] Early finish records actual time spent
- [ ] Undo / "restore number"
- [ ] Optional 25/5 cycle with auto-break

## v0.4 — Widgets
- [ ] 📅 Sudá / lichá week parity (anchor = first Monday of the semester)
- [ ] 🗓 Schedule photo
- [ ] ✅ Habits: daily check, 🔥 streak, weekly progress 0–7
- [ ] 📚 Books: pages, progress bar, weekly pages
- [ ] 🎯 Daily focus (one main thing — anti-overwhelm)
- [ ] 📝 Evening reflection: 3 journal lines

## v0.5 — Modes & stats
- [ ] 📊 Performance graph: day / week / month
- [ ] 🎮 Game mode: hide everything, zero load
- [ ] Auto game mode when a fullscreen app launches (Windows API)
- [ ] 🔥 Deep-work button: 50-min pomodoro + game mode in one click
- [ ] 🚀 Autostart (Windows Run registry key)
- [ ] Tray: icon, menu, Windows toast notifications
- [ ] Streak freeze: one missed day per week doesn't break the chain
- [ ] Gentle framing (progress, never "failure")
- [ ] Multi-instance widgets (two identical windows)

## v0.6 — Polish
- [ ] CI: GitHub Actions builds for windows-latest + ubuntu-latest
- [ ] Global hotkeys (RegisterHotKey) for game mode
- [ ] i18n RU/EN
- [ ] DataStore unit tests (QTest)
- [ ] Dark/light theme

## v1.0 — Integrations (deave / bio-tracker / local AI)
- [ ] Local DataBus: localhost HTTP/WebSocket JSON API — single data
      exchange point between applications
- [ ] bio-tracker: pulse/rMSSD from Amazfit Band 7 → "you're tired, rest"
      widget, morning readiness index, color indication
- [ ] deave (Tauri/Rust/FastAPI): shared data, Second Brain dashboard
- [ ] Ollama (local GPU): evening day summary in your own words,
      study advice, "ask about your progress" mini-chat
- [ ] Exam countdown + university grades/credits
- [ ] Per-subject goals: weekly hours, progress rings
- [ ] Year heatmap (GitHub-style) of study minutes
- [ ] CSV / iCal export
- [ ] Optional SQLCipher encryption
- [ ] Reminders: water, posture, eyes (paired with bio-tracker)

## Principles
- Local & private: everything on disk, no cloud
- One widget = one .cpp file, no edits to foreign code
- Lightweight: a second of CPU per minute, no background services
- Gentle framing: progress only, never failures
- Integration-ready: DataBus API designed in from v0.2
