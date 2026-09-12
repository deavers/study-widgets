# Security and Privacy Policy

StudyWidgets is designed as a local-first personal productivity application.

The project may handle study activity, habits, reflections, schedules,
biometric summaries and local-AI context. Privacy and security are therefore
core design requirements, not optional future features.

## Privacy Defaults

StudyWidgets follows these defaults:

- No cloud account is required.
- No analytics or telemetry are sent by default.
- No hidden network requests are allowed.
- Core functionality works offline.
- Personal data remains on the local device by default.
- External integrations require explicit user opt-in.
- Local AI integration is preferred over cloud AI integration.
- Exporting data is always a deliberate user action.

## Local Data

Current local data includes:

- SQLite study-session database
- Widget settings and positions
- Pomodoro categories
- Future habit, reading and journal data

The default database location is:

```text
%APPDATA%\deavers\StudyWidgets\studywidgets.sqlite
```

The database must never be committed to Git.

## Data That Must Never Be Committed

The following must not be added to the public repository:

- SQLite databases
- Backups
- Personal schedules or timetable images
- Study notes containing personal information
- Reflections or journal entries
- Biometric data
- API tokens
- Passwords
- Private keys
- `.env` files
- Local integration configuration files
- Exported reports containing personal data

The `.gitignore` file contains rules to reduce accidental commits.

## QSettings Policy

Qt QSettings is used for non-sensitive preferences, such as:

- Widget positions
- Enabled widget state
- Visual preferences
- Pomodoro category list

QSettings must not store:

- Passwords
- API tokens
- Encryption keys
- Raw biometric data
- Private journal text

On Windows, secrets should eventually use Windows DPAPI or Windows Credential
Manager instead of QSettings.

## SQLite Policy

SQLite access must follow these rules:

- Use parameterized queries only.
- Never concatenate user input into SQL statements.
- Validate input before writing it to the database.
- Enable SQLite foreign keys for every database connection.
- Back up the database before destructive migrations.
- Add encryption-at-rest as an optional future feature.
- Do not log sensitive database values to the console.

## Local API Policy

Future DataBus, deave and bio-tracker integrations must:

- Bind only to `127.0.0.1` by default.
- Never listen on public network interfaces by default.
- Require a local authentication token.
- Use API versioning.
- Provide an explicit enable/disable switch.
- Log integration metadata without logging sensitive payloads.
- Use rate limiting and input validation.
- Require confirmation before sending personal context to another app.

## Local AI Policy

Future Ollama integration must:

- Remain fully optional.
- Clearly show when local AI is enabled.
- Show what data will be sent to the model.
- Never require a cloud model for core functionality.
- Avoid sending raw biometric data unless explicitly approved.
- Use local models by default.

## Security Limits

StudyWidgets can reduce accidental data exposure, but cannot protect against:

- Malware running under the same Windows account
- A compromised or unlocked device
- An administrator with full system access
- Manual copying of files by the user
- Backups created outside the application
- Screenshots or screen-recording software

For stronger device-level protection, use:

- A strong Windows account password
- Windows Hello / device lock
- BitLocker full-disk encryption
- Regular Windows updates
- Trusted antivirus or endpoint protection

## Reporting a Vulnerability

Do not publish sensitive vulnerability details in a public GitHub issue.

For a personal project, document the issue privately first, reproduce it,
remove sensitive information from logs and create a fix before publishing
technical details.