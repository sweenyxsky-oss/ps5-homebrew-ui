# Native PS5 game library: implementation plan

This work belongs in the native OpenGL PS5 app in `ps5-homebrew-ui`. The
`ps5-library` fork is a separate payload whose documented tile opens a library
in the PS5 browser. The fork contains only its README, payload catalog, and
screenshots; it has no payload source to modify. That is not the target for
this native interface.

## Branch progress

The native interface is registered in the design switcher as
**PS5 Library**. D-pad navigation moves focus, Cross opens a
title or reports that its installer is unavailable, Circle goes back, Square
opens the source chooser, Triangle switches cover/block layout, Options
switches Library/Installed, and left/right changes the package tab on a title.

The screen now has a native loopback client for the documented ShadowMount
Plus `/api/v1/games` route. It fetches on a background thread and parses a
bounded JSON snapshot into title ID, title name, source path/type, version,
installed state, and optional measured size. A malformed response leaves the
last valid catalog in place. The installed filter, scrolling, covers/blocks,
and title details work with live entries. The client always targets
`127.0.0.1:10101`; it never sends game data or credentials over the LAN.

The native catalog parser and behavior cases now cover the real response field
names, escaped Unicode, invalid IDs, API errors, and preservation of the last
good index. The built-in preview data remains available until the user selects
the ShadowMount source and completes a successful refresh. ShadowMount does
not list base-game/DLC/update package files or expose a general package install
route, so those actions stay unavailable. Direct PS5 folder browsing, a
persistent local index, NFS, and package installation are not implemented.

## Five milestones

1. **Native library screen.** Implemented: controller-first Installed/Library
   modes, cover/block layouts, eight-title paging, source dialog, live indexed
   entries, and title details.
2. **Title metadata and packages.** Partially implemented: title ID, name,
   source path/type, version, installed state and measured size parse from the
   real API response. API icon retrieval and actual base-game/DLC/update
   package inventory remain to be connected.
3. **Sources and cached indexing.** Partially implemented: the native app can
   read the existing ShadowMount index from its loopback API without blocking
   the frame loop. Direct folder selection, NFS credentials/transport and a
   persistent metadata cache remain unimplemented. No credentials are stored.
4. **Install actions.** Pending a verified PS5 installer API. ShadowMount
   exposes title mounting and source management, but no game/DLC/update PKG
   installation endpoint; those are not interchangeable operations.
5. **Native and hardware validation.** Host tests, snapshots, native build,
   and real-console checks remain to be run. WSL starts, but its Ubuntu image
   has no Make/Clang/Ninja and sudo requires interactive authentication.
   Console-only API connectivity, controller behavior, filesystem permissions,
   and firmware compatibility have not been established.

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

The native screen now consumes real ShadowMount title snapshots, while sample
records are explicitly marked until a successful refresh. Directory browsing,
persistent index storage, NFS transport, package enumeration and installation
still require additional platform services and a compatible PS5 backend. Each
unavailable action remains labelled; only console validation can establish
the runtime behavior of the API client.
