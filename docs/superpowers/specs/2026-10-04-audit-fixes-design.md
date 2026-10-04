# Audit Fixes Design

The user approved the nine-point repair proposal on 2026-10-04. Keep version 0.2.14, preserve earlier team-import changes, and do not commit, publish or create installers.

## Decisions

- Reconnect unexpected post-handshake danmaku closures with existing bounded retry policy. Stop and authentication closures remain excluded.
- Redact complete credential fields, quoted JSON values and remote URLs before both file logging and previous-handler forwarding. Test with synthetic values only.
- Preserve semaphore occupancy until cancelled synchronous work finishes; suppress its response. Keep shutdown bounded.
- Protect every member implicated in ambiguous import rows before moving any member.
- Render all guild leaders separately before teams, including unassigned leaders.
- Hydrate exact roster matches with room metadata. Broad name queries merge roster hints with remote results. A hint is explicitly unknown, never fabricated offline; maintain C++/Python protocol compatibility and test both sides.
- Refresh only guild live state while navigation is visible, once per minute; preserve cached identities/avatars and recent-open TTL.
- Expose a persistent unsaved warning and retry command. Keep in-memory changes on failure; only clear the warning after a successful sync. No user settings are used in tests.
- Replace compiled role/team fallback with a versioned local event mapping editable in Settings. Default mapping preserves current roles. Match mapped members through verified roster name/room aliases. Do not invent a remote role field or claim remote role synchronization.

## Acceptance

Each defect has a regression that fails before its fix. Run Release and beta24 builds, service/protocol, controller/store, QML interaction/visual/close regressions and executable self-tests. Existing visual mismatches must be inspected; do not automatically replace baselines. Cold installation, external live schema and prolonged real-stream performance remain separate acceptance work.
