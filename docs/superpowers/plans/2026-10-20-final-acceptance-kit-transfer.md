# Final Acceptance Kit Transfer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended) to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a new self-contained Windows acceptance kit that includes installer verification, external SHA-256 anchoring, lifecycle capture/comparison tools, documentation, and an auditable manifest, then verify it after relocation.

**Architecture:** Copy the frozen Step 75 derived kit into a new directory without modifying historical inputs. Rebuild the package manifest from the copied files, add an external freeze record outside the kit, and run package, lifecycle, relocation, cross-working-directory, and tamper tests only against fresh output directories.

**Tech Stack:** Windows PowerShell 5.1, JSON, SHA-256 file hashing, existing acceptance-kit verifier/test scripts, and the Step 75 lifecycle PowerShell tools.

---

### Task 1: Inspect and freeze inputs

- [ ] Read Steps 73–75 reports, inspect the Step 75 kit, record Git state, drive space, and source hashes.
- [ ] Confirm the frozen installer identity from the actual source kit and manifest before copying.

### Task 2: Create the final kit

- [ ] Copy the Step 75 derived kit to a new unique final-kit directory.
- [ ] Rebuild `artifact-manifest.json` from the final directory with normalized, sorted relative paths, roles, required flags, and source identifiers; exclude the manifest itself and test outputs.
- [ ] Add a final-kit manifest validation helper/test only if the copied tools do not already validate the complete manifest.
- [ ] Update README and runbook to describe the unified verifier, lifecycle tools, run IDs, evidence output, manual actions, and evidence limits.
- [ ] Write an external freeze record outside the kit containing package and key-file hashes, counts, sizes, and generation time.

### Task 3: Verify the final kit in place

- [ ] Run PowerShell AST parsing for every shipped `.ps1`.
- [ ] Run the acceptance-kit test, lifecycle synthetic test, and external-hash portability test with fresh outputs.
- [ ] Verify manifest entries against actual files and record installer hash/size.

### Task 4: Relocate and verify independently

- [ ] Copy the final kit to a fresh relocation directory and compare every shipped file by normalized path, size, and SHA-256.
- [ ] Run verifier and lifecycle tests from parent, unrelated, space-containing, and non-ASCII working directories, using fresh output roots.
- [ ] Create a further tamper copy for installer-only, synchronized installer/manifest, tool, README/runbook, missing-tool, malformed-manifest, and path-traversal cases; verify expected nonzero results and preserve diagnostics.

### Task 5: Report and close

- [ ] Re-run `git diff --check`, confirm no staged changes and historical kits unchanged.
- [ ] Save `step76-final-acceptance-kit-transfer-report.md` with all commands, exit codes, counts, hashes, skipped symlink limitation, and unverified real-install boundaries.
