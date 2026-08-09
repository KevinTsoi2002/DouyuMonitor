# Packaged Performance Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a repeatable, privacy-safe Playwright Electron runner for measuring packaged 1/4/6/9-room playback, layout health, and process resource use.

**Architecture:** Keep the production application unchanged. A Node ESM runner launches the unpacked executable with an isolated temporary user-data directory, adds numeric room IDs through the real renderer UI, samples fixed-schema DOM and Electron process metrics, and writes sanitized JSON plus screenshots to the operating-system temporary directory. Pure parsing and metric aggregation live in a small helper module covered by Vitest.

**Tech Stack:** Node.js ESM, Playwright Electron, Electron `app.getAppMetrics()`, Vitest, Windows x64 packaged build.

**Constraints:** Never include playback URLs, query strings, tokens, cookies, credentials, request headers, raw sidecar output, or raw exception objects in reports. Do not change the existing CDN allowlist or playback resolver. The workspace is not a Git repository, so commit steps are intentionally omitted.

---

### Task 1: Performance configuration and aggregation helpers

**Files:**
- Create: `scripts/performance-utils.mjs`
- Create: `tests/performance-utils.test.ts`

- [ ] **Step 1: Write failing tests for validated room IDs and profiles**

```ts
import { describe, expect, it } from 'vitest';
import {
  parseProfileCounts,
  parseRoomIds,
  summarizeMetricSamples,
} from '../scripts/performance-utils.mjs';

describe('performance baseline helpers', () => {
  it('accepts unique numeric room ids without exposing arbitrary text', () => {
    expect(parseRoomIds('2448877, 5526219,2448877,844947')).toEqual([
      '2448877',
      '5526219',
      '844947',
    ]);
    expect(() => parseRoomIds('2448877,https://example.test/live.flv?token=secret'))
      .toThrow('Room IDs must contain digits only');
  });

  it('accepts unique concurrency profiles between one and nine', () => {
    expect(parseProfileCounts('1,4,6,9,4')).toEqual([1, 4, 6, 9]);
    expect(() => parseProfileCounts('0,10')).toThrow('Profiles must be integers from 1 to 9');
  });

  it('summarizes total CPU and working-set samples', () => {
    expect(summarizeMetricSamples([
      { cpuPercent: 10, workingSetBytes: 100, privateBytes: 80 },
      { cpuPercent: 30, workingSetBytes: 140, privateBytes: 120 },
    ])).toEqual({
      sampleCount: 2,
      averageCpuPercent: 20,
      peakCpuPercent: 30,
      averageWorkingSetBytes: 120,
      peakWorkingSetBytes: 140,
      peakPrivateBytes: 120,
    });
  });
});
```

- [ ] **Step 2: Run the targeted test and verify RED**

Run: `npm test -- --run tests/performance-utils.test.ts`

Expected: FAIL because `scripts/performance-utils.mjs` does not exist.

- [ ] **Step 3: Implement the minimal helpers**

```js
const ROOM_ID_PATTERN = /^\d{1,20}$/;

export function parseRoomIds(value) {
  const roomIds = [...new Set(String(value ?? '')
    .split(',')
    .map((item) => item.trim())
    .filter(Boolean))];
  if (roomIds.length === 0) throw new Error('At least one room ID is required');
  if (roomIds.some((roomId) => !ROOM_ID_PATTERN.test(roomId))) {
    throw new Error('Room IDs must contain digits only');
  }
  return roomIds;
}

export function parseProfileCounts(value) {
  const counts = [...new Set(String(value ?? '1,4,6,9')
    .split(',')
    .map((item) => Number(item.trim())))];
  if (counts.some((count) => !Number.isInteger(count) || count < 1 || count > 9)) {
    throw new Error('Profiles must be integers from 1 to 9');
  }
  return counts;
}

const round = (value) => Math.round(value * 100) / 100;

export function summarizeMetricSamples(samples) {
  if (samples.length === 0) throw new Error('At least one metric sample is required');
  const average = (key) => round(samples.reduce((sum, sample) => sum + sample[key], 0) / samples.length);
  const peak = (key) => Math.max(...samples.map((sample) => sample[key]));
  return {
    sampleCount: samples.length,
    averageCpuPercent: average('cpuPercent'),
    peakCpuPercent: peak('cpuPercent'),
    averageWorkingSetBytes: average('workingSetBytes'),
    peakWorkingSetBytes: peak('workingSetBytes'),
    peakPrivateBytes: peak('privateBytes'),
  };
}
```

- [ ] **Step 4: Run the targeted test and verify GREEN**

Run: `npm test -- --run tests/performance-utils.test.ts`

Expected: 3 tests pass.

### Task 2: Packaged Electron performance runner

**Files:**
- Create: `scripts/performance-baseline.mjs`
- Modify: `package.json`
- Modify: `tests/package-windows.test.ts`

- [ ] **Step 1: Extend the package configuration test first**

Add this assertion to the existing Windows packaging test:

```ts
expect(packageConfig.scripts['test:performance']).toBe(
  'node scripts/performance-baseline.mjs',
);
```

- [ ] **Step 2: Run the package test and verify RED**

Run: `npm test -- --run tests/package-windows.test.ts`

Expected: FAIL because `test:performance` is not defined.

- [ ] **Step 3: Add the package script**

```json
"test:performance": "node scripts/performance-baseline.mjs"
```

- [ ] **Step 4: Implement the runner with a fixed sanitized schema**

The runner must:

1. Read `DOUYU_PERF_ROOM_IDS`, `DOUYU_PERF_PROFILES`, `DOUYU_PERF_SAMPLE_MS`, and optional `DOUYU_PERF_EXECUTABLE`.
2. Default the executable to `release/win-unpacked/DouyuMonitor.exe` and reject a missing file before launch.
3. Launch every requested profile separately with `--user-data-dir=<unique os.tmpdir path>` and verify Electron reports that isolated path.
4. Add the first N room IDs through the visible `添加直播间` dialog and `.search-result` item; capture time from result click until that tile has a `<video>` with `readyState >= 2` and advancing `currentTime`.
5. Capture only fixed fields: room ID, success boolean, first-frame milliseconds, video count, playing count, tile count, geometry overlap count, console error/warning counts, page error count, and aggregate process CPU/memory values.
6. Sum `app.getAppMetrics()` process CPU and memory values on a two-second cadence. Discard the warm-up sample and summarize the rest with `summarizeMetricSamples`.
7. Save one screenshot per profile and one JSON report beneath a newly created operating-system temporary directory.
8. Close the Electron application in `finally` and retain the temporary evidence directory; never include raw URLs, requests, headers, cookies, localStorage contents, sidecar stdout/stderr, or raw exception messages in the report.
9. Set a nonzero exit code when a requested profile lacks enough room IDs, a room fails to reach playable state, tile geometry overlaps, or a page/console error occurs. Warnings are recorded but do not fail the run.

The JSON report shape is:

```js
{
  schemaVersion: 1,
  generatedAt: new Date().toISOString(),
  executable: path.basename(executablePath),
  sampleDurationMs,
  profiles: [{
    requestedRoomCount,
    roomIds,
    launchSucceeded,
    isolatedUserData: true,
    rooms: [{ roomId, playable, firstFrameMs }],
    renderer: {
      tileCount,
      videoCount,
      playingCount,
      overlapCount,
      consoleErrorCount,
      consoleWarningCount,
      pageErrorCount,
    },
    processes: summarizeMetricSamples(samples),
    screenshotFile: path.basename(screenshotPath),
    passed,
  }],
}
```

- [ ] **Step 5: Run the targeted tests**

Run: `npm test -- --run tests/performance-utils.test.ts tests/package-windows.test.ts`

Expected: all targeted tests pass.

- [ ] **Step 6: Run type and syntax checks**

Run: `node --check scripts/performance-baseline.mjs`

Run: `node --check scripts/performance-utils.mjs`

Run: `npm run typecheck`

Expected: every command exits 0.

### Task 3: Real packaged baseline and documentation

**Files:**
- Modify: `README.md`
- Update externally: existing Notion development design page

- [ ] **Step 1: Discover nine currently online rooms using only public room metadata**

Record only numeric room IDs and availability outcomes. Do not record playback URLs or response payloads.

- [ ] **Step 2: Run short 1/4/6/9 packaged baselines**

```powershell
$env:DOUYU_PERF_ROOM_IDS='<nine verified numeric room ids>'
$env:DOUYU_PERF_PROFILES='1,4,6,9'
$env:DOUYU_PERF_SAMPLE_MS='15000'
npm run test:performance
```

Expected: the runner emits a temporary JSON report path and exits 0 only when every requested room is playable, the renderer has no errors, and tiles do not overlap.

- [ ] **Step 3: Document the reusable commands and truthful limits**

README must state:

- Build `release/win-unpacked` before running the performance command.
- Room IDs must be currently online and are supplied through `DOUYU_PERF_ROOM_IDS`.
- The short baseline is not the two-hour stability acceptance test.
- The 2-hour 4-room command uses `DOUYU_PERF_PROFILES=4` and `DOUYU_PERF_SAMPLE_MS=7200000`.
- Reports are sanitized and written outside the repository.

- [ ] **Step 4: Run full verification**

Run: `npm test`

Run: `npm run typecheck`

Run: `npm run build`

Run: `npm audit --omit=dev`

Expected: tests, typecheck, build, and production dependency audit all pass.

- [ ] **Step 5: Update Notion only with measured evidence**

Append a dated “多路性能基线” section to the project development design page. Include the exact room counts, sample duration, pass/fail outcome, CPU/memory summary, report location category (system temporary directory, not the stream URL), and unresolved two-hour/Windows 10/display-scaling gates. Fetch the page again after updating to verify the section exists.

