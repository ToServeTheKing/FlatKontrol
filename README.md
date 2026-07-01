# FlatKontrol

A Flatpak permissions manager built the KDE way - C++ with a QML/Kirigami
frontend, ECM/CMake, and KDE Frameworks 6. Think [Flatseal], reimagined as if
the team behind [Elisa] had written it.

It reads each application's baseline permissions from its bundle `metadata`,
layers the global and per-application override files on top (the same
original → global → user resolution Flatseal uses), and writes changes back to
`~/.local/share/flatpak/overrides/`. Dynamic portal permissions are edited over
D-Bus via `org.freedesktop.impl.portal.PermissionStore`.

## Features

- Share (network, IPC), Sockets, Devices, Features - toggles
- Filesystem presets + custom paths (with `:ro`/`:rw`/`:create` and negation)
- Persistent home-relative directories
- Environment variables
- Session / System D-Bus name policies (talk / own)
- Portals (background, notifications, microphone, speakers, camera, location)
- Per-application reset with undo, and global defaults via the "All Applications" entry
- Live refresh when applications are installed or removed

## Install

Tagged releases are built by CI into a single-file bundle (attached to each
[GitHub Release]) and a hosted Flatpak repository on GitHub Pages:

```sh
flatpak remote-add --if-not-exists --user --no-gpg-verify flatkontrol \
  https://toservetheking.github.io/FlatKontrol/flatkontrol.flatpakrepo
flatpak install --user flatkontrol io.github.toservetheking.FlatKontrol
```

(The repository is currently unsigned, hence `--no-gpg-verify`.)

Or install the downloaded bundle directly:

```sh
flatpak install --user ./io.github.toservetheking.FlatKontrol.flatpak
```

[GitHub Release]: https://github.com/toservetheking/FlatKontrol/releases

## Building

Requires Qt 6, KDE Frameworks 6, Kirigami, Kirigami Addons and the CMake
toolchain. On Arch/CachyOS:

```sh
sudo pacman -S --needed cmake extra-cmake-modules base-devel \
    qt6-base qt6-declarative kirigami kirigami-addons \
    ki18n kcoreaddons kiconthemes kcrash kdbusaddons kitemmodels qqc2-desktop-style
```

Then:

```sh
cmake -B build -G Ninja
cmake --build build
./build/bin/flatkontrol
```

## Architecture

- `keyfile.{h,cpp}` - minimal GKeyFile/flatpak-compatible INI reader/writer.
- `flatpakinstallations.{h,cpp}` - installation discovery and per-app metadata/icon.
- `applicationsmodel.{h,cpp}` - sidebar model (`QAbstractListModel`, `QML_ELEMENT`).
- `permissioncatalog.{h,cpp}` - the static catalog of categories and options.
- `portalsbackend.{h,cpp}` - QtDBus PermissionStore client.
- `permissionscontroller.{h,cpp}` - per-app state, resolution, and the row model.
- `qml/` - Kirigami UI.

[Flatseal]: https://github.com/tchx84/Flatseal
[Elisa]: https://invent.kde.org/multimedia/elisa
