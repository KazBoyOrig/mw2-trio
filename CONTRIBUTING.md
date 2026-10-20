# Contributing

MW2 Trio is version **0.0.1-dev**, a pre-alpha developer prototype. There is no playable three-player campaign yet. Keep that status clear in documentation, screenshots and proposed releases.

1. Work on a complete independent test copy. Preserve the original installation and saves. Keep writable files separate from hard links.
2. Verify the full executable fingerprint before using native addresses. Reject unknown builds and changed live bytes before any write.
3. Treat native storage, session accessors, indices, readiness and packet formats as one coordinated design. A reserved third record does not authorize the original two-member APIs to visit it.
4. Establish the documented pre-engine/thread barrier, restore page protections, handle rollback failure and keep injected code/storage alive for escaped pointers. Preserve failure checks.
5. Run `tests/run-tests.ps1` with the explicit x86 compiler. Add meaningful negative cases for changed invariants; avoid tests that only repeat the implementation.
6. Use real game clients and independent native input streams before claiming network gameplay. Scripted teleport/follow behavior and lobby counts are insufficient.

Keep contributions in English. Preserve MIT notices and identify adapted upstream work. Submit only source, small patch contexts and sanitized evidence: no executable/runtime/game assets, full extracted/decompiled game scripts, saves, tokens, personal contact details or owner-specific paths.

For bugs, include the project version, executable hash, observed mode/player count, expected behavior and a minimal reproduction. Summarize relevant log fields; review attachments for private data before uploading. A helper failure, game component failure and multiplayer synchronization failure are different scopes and should be described precisely.

Before a playable release, the roadmap's three-client activation/input, protocol-version, reconnect, fourth-member, revive, checkpoint and mission gates must have actual results. See [ROADMAP.md](docs/ROADMAP.md) and [TESTING.md](docs/TESTING.md).
