Ringbinder now looks and works like a real ring binder, modelled on the paper organizers (and Lotus Organizer) it takes after.

## What's new

- **Tabs on both edges.** Like the dividers in a binder, the tabs of the sections you have turned past move to the left-hand edge and the ones still to come stay on the right. Labels run along the tabs.
- **Two-page spreads.** Every section except the Planner is an open spread with the rings down the middle:
  - **To Do:** the task list on the left, with All / Overdue / Current / Future / Completed tabs down the page edge, and the selected task as a card on the right.
  - **Address:** contacts on the left with A–Z thumb tabs, the selected contact's card on the right.
  - **Calls:** the call list on the left, the selected call on the right.
  - **Notepad:** contents on the left, the open page on the right.
  - **Anniversary:** half a year per spread, three months to a page, with ages and how soon each date comes up.
- **Planner.** A single full-width page punched for the rings, with the columns lined up by weekday as on a paper year planner, so weekends form vertical stripes. The 1st of each month has a red corner.

Your data carries over unchanged.

## Install

```sh
pkgman install ./ringbinder-1.1.0-1-x86_64.hpkg
```

Requires Haiku R1/beta6 or later, plus libcurl (pkgman installs it if needed).

## Tested

- **Renku OS** nightly build00055 (hrev60072+253), x86_64, with this package: installed over 1.0.1 with existing data, which shows in every section; a fresh first run shows empty sections and the welcome page; switching through all seven sections; no output on stdout/stderr.
- With the same source, built outside the package on Renku: To Do sub-tabs filter tasks; adding a contact shows its card; Planner drag-to-add picks the right dates; the window fits a 1024×768 screen on every section.
- Built on Haiku R1/beta6 (hrev59866). Sync engine unit tests pass.
- **Not tested:** signing in to a real Google account and syncing against it.

Check downloads with `sha256sum -c SHA256SUMS`.
