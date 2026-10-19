First release of Ringbinder, a tabbed personal organizer for Haiku and Renku OS: Calendar (week at a glance), To Do, Address, Calls, Planner, Notepad and Anniversary, with optional two-way sync with Google Calendar and Google Contacts.

## Install

```sh
pkgman install ./ringbinder-1.0.0-1-x86_64.hpkg
```

Requires Haiku R1/beta6 or later, plus libcurl (pkgman installs it if needed).

## Tested

- **Renku OS** nightly build00055 (hrev60072+253), x86_64: package installs, app signature and attributes intact, Deskbar entry present. Exercised: Calendar (add appointment, week navigation), To Do, Planner (drag to add an event), Notepad, Anniversary (shown on the Calendar with age), Google sync settings window, quit and relaunch with all data kept. No output on stdout/stderr.
- Built on Haiku R1/beta6 (hrev59866).
- Sync engine: unit tests against a simulated Google pass, on Linux and on Haiku.
- **Not tested:** signing in to a real Google account and syncing against it.

Check downloads with `sha256sum -c SHA256SUMS`.
