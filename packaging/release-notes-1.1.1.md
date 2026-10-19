Ringbinder now has a VitruvianOS package. The program is the same as 1.1.0; this release only adds the `.deb`.

## New

- **VitruvianOS:** `ringbinder_1.1.1-1_amd64.deb` for VitruvianOS 0.6.x.

```sh
sudo apt install ./ringbinder_1.1.1-1_amd64.deb
```

It adds Ringbinder to the Deskbar's Applications menu and as `ringbinder` on the command line. It is built for VitruvianOS 0.6.2 and refuses to install on 0.7 or later, because the Haiku API is C++ and class layouts can change between releases.

- The Makefile now builds on VitruvianOS as well as Haiku, and `tools/vos-sdk/` can build a link-time SDK so the `.deb` can be built on any Linux machine.

## Haiku and Renku OS

There is no new Haiku package in this release: use `ringbinder-1.1.0-1-x86_64.hpkg` from the [1.1.0 release](https://github.com/idumych/ringbinder/releases/tag/v1.1.0). The program is unchanged.

## Tested

- **VitruvianOS: not tested.** The `.deb` has not been installed or run on VitruvianOS. What was checked: it compiles and links against the VitruvianOS 0.6.2 libraries (built from source as a link-time SDK); the package's control fields and file list are as intended (its dependency names, including libcurl, have not been checked against a VitruvianOS install); the app signature is in the binary and the attributes that postinst restores (signature, flags, version, file type) are in the package; and the binary needs only glibc 2.38 and old libstdc++ symbols, which Debian trixie provides. Please report anything that doesn't work.
- The Makefile change was not re-run on Haiku for this release.

Check the download with `sha256sum -c SHA256SUMS`.
