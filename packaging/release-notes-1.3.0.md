Ringbinder now runs on Linux, as an AppImage. The Haiku, Renku OS and VitruvianOS packages are unchanged.

## New

- **Linux:** `Ringbinder-1.3.0-x86_64.AppImage`. Make it executable and run it; there is nothing to install.

```sh
chmod +x Ringbinder-1.3.0-x86_64.AppImage
./Ringbinder-1.3.0-x86_64.AppImage
```

- It needs an x86-64 distribution with **glibc 2.43 or newer** (Ubuntu 26.04, or a current rolling distribution such as Arch). It bundles the [Cosmoe](https://github.com/haydentech/cosmoe) library (the Haiku API on Linux), whose release packages are only built for that baseline.
- Your data is kept in `~/.config/cosmoe/Ringbinder/organizer.data`.
- The AppImage is built by GitHub Actions (`.github/workflows/appimage.yml`) from the source in this repository.
- The Makefile now also builds on Linux against Cosmoe (`make appimage`, or see the README).

## Fixed

- Linux only: the section tab labels are placed correctly. Cosmoe positions rotated text differently from Haiku, which pushed the labels out of their tabs; the code compensates for it on Linux and is unchanged on Haiku and VitruvianOS.

## Haiku, Renku OS and VitruvianOS

There are no new packages in this release. Use `ringbinder-1.1.0-1-x86_64.hpkg` (Haiku, Renku OS) from the [1.1.0 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.0) or `ringbinder_1.1.1-1_amd64.deb` (VitruvianOS) from the [1.1.1 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.1). The web version is in [1.2.2](https://github.com/idumych/ringbinder/releases/tag/v1.2.2).

## Tested

- **What was run:** the exact AppImage attached here, as built by CI from commit `df8ba8a`, on X11 (Xvfb with Openbox). It started with an empty log, and clicking each section tab (forward through all seven, then back along the left-hand edge) turned to the right section with the labels correctly placed. Only the tabs were exercised.
- **Not tested:** Wayland; the rest of the app's features on Linux; Google sign-in and sync on Linux (the sign-in page is opened with `xdg-open`); desktop notifications (sent with `notify-send`); any distribution other than the one it was built and run on.
- **Not re-run:** the Makefile was restructured to add the Linux branch. The Haiku and VitruvianOS builds were not re-run for this release.

Check the download with `sha256sum -c SHA256SUMS`.
