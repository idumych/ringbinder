A small fix to the web version's section tabs. The native program and packages are unchanged.

## Fixed

- **Web version:** the tabs on the left-hand edge were drawn with their rounded corners on the wrong side. They now curve on the outer edge of the binder, as in the native app.
- **Web version:** the Anniversary tab clipped its label. Tabs now have a minimum height, so "Anniversary" fits, and the binder scrolls in windows shorter than 700px rather than squashing the tabs.

## Downloads

- `ringbinder-web-1.2.1.zip`: unzip it and open `index.html`, or copy the folder to any static host. See the [1.2.0 notes](https://github.com/idumych/ringbinder/releases/tag/v1.2.0) for what the web version does and doesn't include.
- Haiku, Renku OS and VitruvianOS: no new packages. Use `ringbinder-1.1.0-1-x86_64.hpkg` from the [1.1.0 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.0) or `ringbinder_1.1.1-1_amd64.deb` from the [1.1.1 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.1).

## Tested

- Checked with headless Firefox screenshots at 800px and 560px window heights. Not tried in Chrome or Safari, or on a phone.

Check the download with `sha256sum -c SHA256SUMS`.
