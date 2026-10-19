Ringbinder now has a web version. It runs in a browser, with no install and no server-side code.

## New

- **Web version:** `ringbinder-web-1.2.0.zip`. Unzip it and open `index.html`, or copy the folder to any static host. It is plain HTML, CSS and JavaScript with no build step and no dependencies.
- All seven sections, with the same binder look (leather cover, tabs that move to the left edge as you turn past them, rings), the same To Do and Address filtering and sorting, and keyboard shortcuts (`1`–`7`, `N`, `E`, `D`, `K`, arrows, `T`).
- Data stays in your browser's `localStorage`. **Export…** saves the whole binder as a JSON file and **Import…** replaces the binder with one, for backups or moving between browsers.

## Not in the web version

- Google Calendar and Contacts sync.
- Reading the native app's `organizer.data` file. The web export is its own JSON format.

## Planned

- A self-hosted version with user login, so a binder can live on your own server instead of in one browser.

## Haiku, Renku OS and VitruvianOS

There are no new native packages in this release; the native program is unchanged from 1.1.1. Use `ringbinder-1.1.0-1-x86_64.hpkg` (Haiku, Renku OS) from the [1.1.0 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.0) or `ringbinder_1.1.1-1_amd64.deb` (VitruvianOS) from the [1.1.1 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.1).

## Tested

- Tested in headless Firefox only: a script drove the real page (section switching, To Do tabs, Address letters, add/edit/delete with validation, Planner drag-to-create, notepad autosave, import/export including a bad file). Not tried in Chrome or Safari, on a phone, or with a real mouse.
- The narrow-screen layout (under about 860px) is basic and untested on a real device.

Check the download with `sha256sum -c SHA256SUMS`.
