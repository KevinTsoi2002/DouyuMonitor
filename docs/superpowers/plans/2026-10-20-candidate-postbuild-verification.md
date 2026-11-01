# Candidate Postbuild Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a read-only, repeatable verification command that compares a finished installer candidate's current Stage and installer file with its saved build evidence.

**Architecture:** Extend the existing build-result verifier with an internal mode that validates the saved result/manifests without requiring the current installer identity, then add a separate candidate verifier that checks the current Stage against the saved pre-Inno manifest and independently checks the installer path, size, and SHA-256. The verifier writes a unique caller-selected evidence JSON and returns zero only for `verified`; all drift, invalid evidence, missing evidence, and I/O failures return non-zero.

**Tech Stack:** PowerShell 5+/7, existing `stage-integrity.ps1`, existing `verify-installer-build-result.ps1`, isolated synthetic files and manifests.

---

### Task 1: Extend the existing result verifier with a baseline-only mode

**Files:**
- Modify: `native/scripts/verify-installer-build-result.ps1`
- Test: `native/scripts/test-installer-build-result.ps1`

- [ ] **Step 1: Add an optional `-SkipInstallerIdentity` switch**

Keep the current default behavior unchanged. When the switch is set, still validate the expected installer path and the recorded installer metadata shape, but do not require the current installer file to exist or match the recorded bytes/hash. This lets the postbuild verifier classify current installer drift separately while reusing the existing result/manifests contract.

- [ ] **Step 2: Run the existing result-contract test**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\native\scripts\test-installer-build-result.ps1 -EvidenceRoot D:\DouyuAuditS7-b0e669b6-step70-candidate-verification\baseline-contract
```

Expected: exit code `0`; existing default verifier cases remain unchanged.

### Task 2: Implement the read-only candidate verifier

**Files:**
- Create: `native/scripts/verify-installer-candidate.ps1`

- [ ] **Step 1: Define explicit inputs and unique output**

Require `-ResultPath` and `-EvidenceRoot`; reject an evidence root that already exists. Resolve the result path without following reparse points, invoke the existing verifier with `-SkipInstallerIdentity`, and use the result's saved pre-Inno manifest as the immutable baseline.

- [ ] **Step 2: Verify current Stage**

Write a new current manifest under the caller's evidence root using `stage-integrity.ps1 -Mode Write`, then compare it with the saved pre-Inno manifest using `stage-integrity.ps1 -Mode Compare`. Never write to the Stage or update the saved baseline. Preserve `added`, `removed`, and `changed` arrays in the verification result.

- [ ] **Step 3: Verify current installer identity**

Require the recorded path to equal the versioned installer path under the result directory. Check ordinary-file type, reparse-point exclusion, nonzero size, recorded byte count, and recorded SHA-256. Classify a mismatch as `drift_detected`; malformed result fields as `invalid_evidence`.

- [ ] **Step 4: Emit status and exit code**

Write `candidate-verification.json` with `verificationRunId`, timestamps, result/baseline/current paths, manifest identifiers, file counts and bytes, installer identities, diff arrays, status, and reasons. Use `verified`/0 only when all checks pass; use nonzero exits for `drift_detected`, `invalid_evidence`, `insufficient_evidence`, and `verification_error`.

### Task 3: Add isolated candidate-verification coverage

**Files:**
- Create: `native/scripts/test-installer-candidate-verification.ps1`

- [ ] **Step 1: Build independent synthetic candidates**

For each case, create a unique Stage, output, result, and evidence directory. Generate trusted pre/post manifests with the existing helper and create a nonempty text placeholder installer. Never use a historical Stage, Release output, or real installer.

- [ ] **Step 2: Cover success and Stage drift**

Cover: valid candidate (`verified`); removed file (`removed`); changed file (`changed`); added file (`added`); and repeated verification after drift without changing the baseline.

- [ ] **Step 3: Cover installer drift and invalid outputs**

Cover modified installer bytes, missing installer, zero-byte installer, and directory-at-installer-path. Assert nonzero exit and explicit reasons.

- [ ] **Step 4: Cover invalid evidence and isolation**

Cover missing/malformed result JSON, missing required field, contradictory manifests, recorded installer metadata mismatch, invalid/escaped Stage paths, two independent candidate roots, and an optional reparse-point case when the Windows test environment can create one safely.

- [ ] **Step 5: Record machine-readable test results**

Write per-case logs and a summary with checks, passed, failed, status, exit code, and evidence paths. Treat unsupported reparse-point creation as unverified rather than passing it silently.

### Task 4: Run focused and regression verification

**Files:**
- Create: `D:\DouyuAuditS7-b0e669b6-step70-candidate-verification\` evidence files
- Create: `C:\Users\15070\AppData\Local\Temp\DouyuAuditS7-b0e669b6\results\step70-candidate-postbuild-verification-report.md`

- [ ] **Step 1: Run the new isolated test**

Run the new test with a fresh D: evidence root and capture its exit code, summary, and per-case logs.

- [ ] **Step 2: Run existing packaging regressions**

Run `test-stage-integrity.ps1`, `test-stage-drift-injection.ps1`, `test-installer-script.ps1`, `test-installer-build-result.ps1`, `test-installer-output-safety.ps1`, and the existing provenance test entry point with their real project interpreters/parameters.

- [ ] **Step 3: Run PowerShell parse checks and diff checks**

Parse the modified scripts with `System.Management.Automation.Language.Parser`, run `git diff --check`, and confirm the index remains empty.

- [ ] **Step 4: Perform one read-only historical verification**

Use the saved Step 63 `build-result.json`/manifest evidence if available. Do not regenerate a baseline from the current 116-file Stage. Record the expected `drift_detected`, installer identity result, and evidence limitations.

- [ ] **Step 5: Write the Step 70 report**

Document the pre/post Git state, contract, commands, A–I results, historical verification, modified files, test statistics, and the distinction between current-evidence consistency, Stage pre/post consistency, installer internal payload consistency, and real installation success.
