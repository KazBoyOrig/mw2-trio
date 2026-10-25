# Credits and provenance

## cranebeaker and coop-anything

**Special thanks to [cranebeaker](https://github.com/cranebeaker), creator of [coop-anything](https://github.com/cranebeaker/coop-anything).** The project's excellent guides made this investigation much easier to structure: identify the exact build, isolate the installation, audit subsystems, test components, and report evidence honestly.

MW2 Trio follows that playbook. Its initial hub reference was commit [`13475497d67dcd34a9c4490009244db4dc9ecd2e`](https://github.com/cranebeaker/coop-anything/tree/13475497d67dcd34a9c4490009244db4dc9ecd2e). The hub is credited for its methodology and documentation; this credit does not attribute MW2 Trio's native code to its author. Its MIT notice is preserved in [third_party/coop-anything-LICENSE](third_party/coop-anything-LICENSE).

## stxlvn and mw2_coop contributors

[stxlvn/mw2_coop](https://github.com/stxlvn/mw2_coop) is the MIT-licensed two-player campaign project used as technical prior art and a script starting point. The reference is pinned to commit [`8fcb6bcf9980894a631906059558a4797f4c9752`](https://github.com/stxlvn/mw2_coop/tree/8fcb6bcf9980894a631906059558a4797f4c9752).

Adapted components include the export-generator/system-winmm-forwarding approach and the GSC source-buffer/patch-loader approach. The x86 stubs, exact fingerprint guards, native control/storage/cache code, portable SHA-256 and helper tests were developed for this prototype.

`content/framework` includes the pinned upstream's 14 own GSC scripts and 26 patch files, plus an own compiler/player-count probe. The `_load.gsc.patch` retains the complete upstream patch and appends one independent probe block. The unchanged upstream license is included both there and in [third_party/mw2_coop-LICENSE](third_party/mw2_coop-LICENSE). Content hashes and the single upstream patch exception are recorded in [the content manifest](content/framework/manifest.json).

The upstream native mod targets a different x64 executable. Including its source scripts does not mean this x86 prototype inherits its compatibility, gameplay results or two-player support.

## MW2 Trio

Maintained by [KazBoyOrig](https://github.com/KazBoyOrig) and contributors. This version was developed with AI-assisted implementation, code review and local component testing. Verification claims describe tests actually run; they do not assert three-player gameplay.

## Game material and tools

Call of Duty: Modern Warfare 2 and its game assets belong to their respective owners. No game executable, game runtime dependency, full extracted game script or other game asset is distributed here. Patch files contain small game-owned excerpts solely to locate changes; those excerpts are outside the MIT license.

LLVM-MinGW, Ghidra and the JDK were development tools. Their tool binaries are not bundled. No source implementation from the unlicensed legacy COD-MOD tree is redistributed.

Developer DLL bundles contain statically linked compiler/runtime support from the pinned [LLVM-MinGW 20261006 toolchain](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006). The toolchain's unchanged [MinGW-w64 runtime notices](third_party/mingw-w64-runtime-LICENSE) and [LLVM/toolchain license and exceptions](third_party/llvm-mingw-LICENSE) accompany those binaries. These third-party components retain their respective terms; the project's MIT license does not replace them. Keep these notices with redistributed developer bundles.
