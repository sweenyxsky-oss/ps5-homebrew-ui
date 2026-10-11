# Native PS5 game library: implementation plan

This work belongs in the native OpenGL PS5 app in `ps5-homebrew-ui`. The
`ps5-library` fork is a separate payload whose documented tile opens a library
in the PS5 browser. The fork contains only its README, payload catalog, and
screenshots; it has no payload source to modify. That is not the target for
this native interface.

## Branch progress

The native interface preview is registered in the design switcher as
**PS5 Library**. In the preview, D-pad navigation moves focus, Cross opens a
title or reports that its installer is unavailable, Circle goes back, Square
opens the source chooser, Triangle switches cover/block layout, Options
switches Library/Installed, and left/right changes the package tab on a title.

The screen uses clearly labeled sample records. It demonstrates the requested
layout and package information, but does not read a PS5 directory, persist an
index, configure an NFS share, or install a package. Milestones 1 and the
presentation portions of 2 and 3 are implemented as UI preview only. Milestone
4 is pending a supported installer backend. Milestone 5 is pending builds and
hardware validation.

## Five milestones

1. **Native library screen.** Add a controller-first library design to the
   native app, with Installed and Library modes, cover and compact grid views,
   an Installed filter, source affordance, and a selected-title details view.
2. **Title metadata and packages.** Define the title record consumed by the
   screen (title ID, display name, cover, installed state, version, region,
   package groups, format, and byte size). Keep base game, DLC, and updates
   distinct; show exact filenames, extensions, and sizes.
3. **Sources and cached indexing.** Add a source model for the default
   `/hostapp` location, a PS5 folder picker, and NFS connection settings
   (address, export path, user and password). Cache directory metadata and
   invalidate it when the user refreshes or changes a source. Do not cache
   credentials with the game index.
4. **Install actions.** Connect explicit game, DLC, update, and install-all
   actions to a PS5-supported installer backend. Show the selected package
   format and size before confirmation, then report real progress and errors.
5. **Native and hardware validation.** Build the PS5 app, run host snapshots
   and unit tests, and validate scanning, metadata grouping, controller
   navigation, and install behavior on supported console firmware.

## Integration findings

- `ShadowMountPlus` documents `.ffpkg`, `.exfat`, `.ffpfs`, and
  `.ffpfsc` image formats, but it is an auto-mounter. Its format recognition
  can inform labels; mounting behavior must remain behind its own API.
- `ps5-ezremote-client` supports NFS through a `libnfs` client and reads
  remote directory entries, but its documented launcher requires `websrv`
  and its UI is a web UI, so it is not the native UI foundation. In the
  inspected `NfsClient::Connect`, the URL supplies server/export and the
  `user`/`pass` arguments are not used; do not promise credential support
  by reusing it unchanged. It also documents PS5 fPKG installation, which is
  a useful backend reference to investigate separately.
- `RemotePFS` exposes NFS-backed files through an SBC and USB mass storage.
  It targets Linux SBC hardware, not an in-process PS5 NFS mount.
- `fuse-nfs` depends on FUSE; its mount helper is not itself a PS5 NFS client.
  `libnfs` is the relevant protocol library to evaluate for a native client,
  subject to PS5 toolchain and licensing checks.
- `Itemzflow` contains PS5 homebrew and NFS-related code, but its code and
  dependencies need a separate compatibility review before reuse.
- The PS5 Library project description and payload list do not document a
  native UI or a local PS5 filesystem browser API.

## Scope of this branch

The native UI milestone can be implemented and reviewed in this repository.
Real directory scanning, persistent index storage, NFS transport, metadata
parsing, and package installation require platform services and a compatible
PS5 backend; the UI must not represent demo data as a successful scan or
installation. Each backend integration should have a clear capability check
and an honest unavailable/error state.
