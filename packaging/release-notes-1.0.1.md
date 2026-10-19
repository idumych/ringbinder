Bug fix release.

## Fixed

- **Notepad:** New page now starts blank. Before, it created a copy of the page you were on (usually "Welcome to Ringbinder"). Pages created that way are ordinary pages; delete any duplicates you don't want.

## Install

```sh
pkgman install ./ringbinder-1.0.1-1-x86_64.hpkg
```

Requires Haiku R1/beta6 or later, plus libcurl (pkgman installs it if needed). Upgrading from 1.0.0 keeps your data.

## Tested

- **Renku OS** nightly build00055 (hrev60072+253), x86_64: upgrade from 1.0.0 with data kept; New page is blank; typing into it, switching pages and quitting keeps the text. No output on stdout/stderr.
- Built on Haiku R1/beta6 (hrev59866). Sync engine unit tests pass.
- **Not tested:** signing in to a real Google account and syncing against it.

Check downloads with `sha256sum -c SHA256SUMS`.
