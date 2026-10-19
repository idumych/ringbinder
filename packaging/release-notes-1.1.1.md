Ringbinder now has a VitruvianOS package. The program is the same as 1.1.0; this release adds the `.deb` and builds both packages from one commit.

## New

- **VitruvianOS:** `ringbinder_1.1.1-1_amd64.deb` for VitruvianOS 0.6.x.

```sh
sudo apt install ./ringbinder_1.1.1-1_amd64.deb
```

It adds Ringbinder to the Deskbar's Applications menu and as `ringbinder` on the command line. It is built for VitruvianOS 0.6.2 and refuses to install on 0.7 or later, because the Haiku API is C++ and class layouts can change between releases.

- The Makefile now builds on VitruvianOS as well as Haiku, and `tools/vos-sdk/` can build a link-time SDK so the `.deb` can be built on any Linux machine.

## Haiku and Renku OS

```sh
pkgman install ./ringbinder-1.1.1-1-x86_64.hpkg
```

Unchanged apart from the version number. Requires Haiku R1/beta6 or later, plus libcurl.

## Tested

- **VitruvianOS: not tested.** The `.deb` has not been installed or run on VitruvianOS. What was checked: it compiles and links against the VitruvianOS 0.6.2 libraries (built from source as a link-time SDK); the package's control fields and file list are as intended (its dependency names, including libcurl, have not been checked against a VitruvianOS install); the app signature is in the binary and the attributes that postinst restores (signature, flags, version, file type) are in the package; and the binary needs only glibc 2.38 and old libstdc++ symbols, which Debian trixie provides. Please report anything that doesn't work.
- **Haiku and Renku OS:** the package is built and packaged on Haiku R1/beta6 from the same commit, with the sync engine unit tests passing. It has not been re-run on Renku OS for this release; the program's code is the same as 1.1.0, which was tested there (see the 1.1.0 release notes).
- **Not tested:** signing in to a real Google account and syncing against it.

Check downloads with `sha256sum -c SHA256SUMS`.
