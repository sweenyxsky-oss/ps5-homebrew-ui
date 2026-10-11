# Native PS5 game library: implementation plan

This work belongs in the native OpenGL PS5 app in `ps5-homebrew-ui`. The
`ps5-library` fork is a separate payload whose documented tile opens a library
in the PS5 browser; that is not the target for this native interface.

## Five milestones

1. **Native library screen.** Add a controller-first library design to the
   native app, with Installed and Library modes, cover and compact grid views,
   search/filter affordances, and a selected-title details view.
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
