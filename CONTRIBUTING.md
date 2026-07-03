# Contributing to FlatKontrol

## Build

FlatKontrol is a KDE/Kirigami application (C++20 + QML) built with CMake
and Extra CMake Modules. See the README for the full list of system
packages to install, then:

```sh
cmake -B build -G Ninja
cmake --build build
./build/bin/flatkontrol
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

Tests live in `autotests/` and link the core logic directly out of `src/`
(see `autotests/CMakeLists.txt`). CI runs the same suite on every pull
request via `.github/workflows/test.yml`, using `flatpak-builder`'s
`run-tests` option so the run happens inside the same KDE SDK sandbox as
release builds.

## Conventions

- Every source file starts with `SPDX-License-Identifier: GPL-3.0-or-later`
  (in whatever comment syntax fits the file type). Files that can't carry
  an inline header — `.desktop`, `keys/*.asc`, `screenshots/*` — are
  covered instead by `REUSE.toml`.
- `src/` is intentionally flat: one class per concern, no `models/`,
  `controllers/`, or `viewmodels/` subfolders. See the README's
  "Architecture" section for what each class owns.
- C++ backend classes are exposed to QML via `QML_ELEMENT`; QML views are
  meant to stay thin renderers over that state, not hold logic themselves.

## Releasing

`.github/workflows/build.yml` builds, signs, and publishes a Flatpak
bundle whenever a `v*` tag is pushed. To cut a release: bump
`project(... VERSION ...)` in `CMakeLists.txt`, add a matching `<release>`
entry to the metainfo file, update the manifest's pinned `tag:`, then tag
and push.
