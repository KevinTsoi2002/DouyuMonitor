import { mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { basename, dirname, isAbsolute, join, relative, resolve, sep } from 'node:path';
import { performance } from 'node:perf_hooks';
import { fileURLToPath } from 'node:url';
import { _electron } from 'playwright';
import {
  SCREENSHOT_PRIVACY_STYLE,
  parseProfileCounts,
  parseRoomIds,
  parseSampleDurationMs,
  profileLayoutForRoomCount,
  summarizeMetricSamples,
} from './performance-utils.mjs';

const DEFAULT_EXECUTABLE = 'release/win-unpacked/DouyuMonitor.exe';
const SAMPLE_INTERVAL_MS = 2_000;
const ROOM_ACTION_TIMEOUT_MS = 30_000;
const WINDOW_TIMEOUT_MS = 30_000;
const KILOBYTES_TO_BYTES = 1024;
const RESULT_FILE = 'performance-baseline.json';

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');

const emptyRendererMetrics = () => ({
  tileCount: 0,
  videoCount: 0,
  playingCount: 0,
  overlapCount: 0,
  consoleErrorCount: 0,
  consoleWarnCount: 0,
  pageErrorCount: 0,
});

const emptyProcessMetrics = () => ({
  sampleCount: 0,
  averageCpuPercent: 0,
  peakCpuPercent: 0,
  averageWorkingSetBytes: 0,
  peakWorkingSetBytes: 0,
  peakPrivateBytes: 0,
});

function resolveExecutable(value) {
  const configured = value === undefined ? DEFAULT_EXECUTABLE : value.trim();
  if (!configured) throw new Error('INVALID_EXECUTABLE');
  return resolve(projectRoot, configured);
}

function isPathInside(parentPath, childPath) {
  const pathFromParent = relative(resolve(parentPath), resolve(childPath));
  return pathFromParent === ''
    || (!pathFromParent.startsWith(`..${sep}`)
      && pathFromParent !== '..'
      && !isAbsolute(pathFromParent));
}

function setFailure(profile, failureStage, failureCode) {
  if (profile.failureCode !== undefined) return;
  profile.failureStage = failureStage;
  profile.failureCode = failureCode;
}

function createProfile(requestedRoomCount, availableRoomIds) {
  const roomIds = availableRoomIds.slice(0, requestedRoomCount);
  const layout = profileLayoutForRoomCount(requestedRoomCount);
  return {
    requestedRoomCount,
    roomIds,
    layoutId: layout.id,
    layoutSelected: false,
    launchSucceeded: false,
    isolatedUserData: false,
    rooms: roomIds.map((roomId) => ({
      roomId,
      playable: false,
      firstFrameMs: null,
      initialCurrentTime: null,
      finalCurrentTime: null,
      initialDecodedFrames: null,
      finalDecodedFrames: null,
      videoWidth: null,
      videoHeight: null,
      continuedPlayback: false,
    })),
    renderer: emptyRendererMetrics(),
    processes: emptyProcessMetrics(),
    screenshotFile: null,
    passed: false,
  };
}

function attachRendererCounters(page, counters, attachedPages) {
  if (attachedPages.has(page)) return;
  attachedPages.add(page);
  page.on('console', (message) => {
    const type = message.type();
    if (type === 'error') counters.consoleErrorCount += 1;
    if (type === 'warning' || type === 'warn') counters.consoleWarnCount += 1;
  });
  page.on('pageerror', () => {
    counters.pageErrorCount += 1;
  });
}

function remainingTimeout(deadline) {
  return Math.max(1, Math.ceil(deadline - performance.now()));
}

async function addRoomThroughUi(page, roomId) {
  const previousVideoCount = await page.locator('video').count();
  let clickStartedAt;
  try {
    await page.locator('button[aria-label="添加直播间"]').first().click({
      timeout: ROOM_ACTION_TIMEOUT_MS,
    });
    await page.locator('#room-search').fill(roomId, { timeout: ROOM_ACTION_TIMEOUT_MS });
    await page.locator('.room-search-form button[type="submit"]').click({
      timeout: ROOM_ACTION_TIMEOUT_MS,
    });

    const result = page.locator('.search-result').filter({ hasText: roomId }).first();
    await result.waitFor({ state: 'visible', timeout: ROOM_ACTION_TIMEOUT_MS });
    clickStartedAt = performance.now();
    await result.click({ timeout: ROOM_ACTION_TIMEOUT_MS });
  } catch {
    await page.keyboard.press('Escape').catch(() => {});
    return {
      playable: false,
      firstFrameMs: null,
      failureStage: 'room-add',
      failureCode: 'ROOM_SEARCH_FAILED',
    };
  }

  try {
    const deadline = clickStartedAt + ROOM_ACTION_TIMEOUT_MS;
    const video = page.locator('video').nth(previousVideoCount);
    await video.waitFor({ state: 'attached', timeout: remainingTimeout(deadline) });
    await page.waitForFunction(
      ({ videoIndex }) => {
        const target = document.querySelectorAll('video').item(videoIndex);
        return target instanceof HTMLVideoElement && target.readyState >= 2;
      },
      { videoIndex: previousVideoCount },
      { timeout: remainingTimeout(deadline) },
    );

    const initialTime = await video.evaluate((element) => element.currentTime);
    await page.waitForFunction(
      ({ videoIndex, startTime }) => {
        const target = document.querySelectorAll('video').item(videoIndex);
        return target instanceof HTMLVideoElement
          && target.readyState >= 2
          && Number.isFinite(target.currentTime)
          && target.currentTime > startTime + 0.05;
      },
      { videoIndex: previousVideoCount, startTime: initialTime },
      { timeout: remainingTimeout(deadline) },
    );

    return {
      playable: true,
      firstFrameMs: Math.round(performance.now() - clickStartedAt),
    };
  } catch {
    return {
      playable: false,
      firstFrameMs: null,
      failureStage: 'playback',
      failureCode: 'FIRST_FRAME_TIMEOUT',
    };
  }
}

async function selectProfileLayout(page, roomCount) {
  const layout = profileLayoutForRoomCount(roomCount);
  const expectedGrid = page.locator(`.workspace-grid.layout-${layout.id}`);
  if (await expectedGrid.count()) return true;

  try {
    await page.locator('.layout-menu-trigger').click({ timeout: ROOM_ACTION_TIMEOUT_MS });
    await page.locator('.layout-option').filter({ hasText: layout.shortLabel }).first().click({
      timeout: ROOM_ACTION_TIMEOUT_MS,
    });
    await expectedGrid.waitFor({ state: 'visible', timeout: ROOM_ACTION_TIMEOUT_MS });
    return true;
  } catch {
    return false;
  }
}

async function collectProcessMetricSample(electronApp) {
  return electronApp.evaluate(({ app }, kilobytesToBytes) => app.getAppMetrics().reduce(
    (sample, metric) => ({
      cpuPercent: sample.cpuPercent + (Number.isFinite(metric.cpu?.percentCPUUsage)
        ? metric.cpu.percentCPUUsage
        : 0),
      workingSetBytes: sample.workingSetBytes
        + (Number.isFinite(metric.memory?.workingSetSize)
          ? metric.memory.workingSetSize * kilobytesToBytes
          : 0),
      privateBytes: sample.privateBytes
        + (Number.isFinite(metric.memory?.privateBytes)
          ? metric.memory.privateBytes * kilobytesToBytes
          : 0),
    }),
    { cpuPercent: 0, workingSetBytes: 0, privateBytes: 0 },
  ), KILOBYTES_TO_BYTES);
}

function delay(milliseconds) {
  return new Promise((resolveDelay) => setTimeout(resolveDelay, milliseconds));
}

async function sampleProcessMetrics(electronApp, sampleDurationMs) {
  await collectProcessMetricSample(electronApp);
  await delay(SAMPLE_INTERVAL_MS);

  const samples = [];
  const startedAt = performance.now();
  do {
    samples.push(await collectProcessMetricSample(electronApp));
    const remaining = sampleDurationMs - (performance.now() - startedAt);
    if (remaining > 0) await delay(Math.min(SAMPLE_INTERVAL_MS, remaining));
  } while (performance.now() - startedAt < sampleDurationMs);

  return summarizeMetricSamples(samples);
}

async function readRendererMetrics(page, counters) {
  const domMetrics = await page.evaluate(() => {
    const tiles = [...document.querySelectorAll('.room-tile')];
    const videos = [...document.querySelectorAll('video')];
    const rectangles = tiles.map((tile) => tile.getBoundingClientRect());
    let overlapCount = 0;

    for (let leftIndex = 0; leftIndex < rectangles.length; leftIndex += 1) {
      for (let rightIndex = leftIndex + 1; rightIndex < rectangles.length; rightIndex += 1) {
        const left = rectangles[leftIndex];
        const right = rectangles[rightIndex];
        if (
          left.left < right.right
          && left.right > right.left
          && left.top < right.bottom
          && left.bottom > right.top
        ) {
          overlapCount += 1;
        }
      }
    }

    return {
      tileCount: tiles.length,
      videoCount: videos.length,
      playingCount: videos.filter((video) => video instanceof HTMLVideoElement
        && video.readyState >= 2
        && !video.paused
        && !video.ended).length,
      overlapCount,
    };
  });

  return {
    ...domMetrics,
    consoleErrorCount: counters.consoleErrorCount,
    consoleWarnCount: counters.consoleWarnCount,
    pageErrorCount: counters.pageErrorCount,
  };
}

async function readPlaybackMetrics(page, roomCount) {
  return page.evaluate((expectedCount) => [...document.querySelectorAll('video')]
    .slice(0, expectedCount)
    .map((video) => {
      const quality = typeof video.getVideoPlaybackQuality === 'function'
        ? video.getVideoPlaybackQuality()
        : undefined;
      const decoded = Number.isFinite(quality?.totalVideoFrames)
        ? quality.totalVideoFrames
        : Number.isFinite(video.webkitDecodedFrameCount) ? video.webkitDecodedFrameCount : null;
      return {
        currentTime: Number.isFinite(video.currentTime) ? video.currentTime : null,
        decodedFrames: decoded,
        videoWidth: video.videoWidth || null,
        videoHeight: video.videoHeight || null,
      };
    }), roomCount);
}

function applyPlaybackMetrics(profile, initial, final) {
  profile.rooms.forEach((room, index) => {
    const before = initial[index] ?? {};
    const after = final[index] ?? {};
    room.initialCurrentTime = before.currentTime ?? null;
    room.finalCurrentTime = after.currentTime ?? null;
    room.initialDecodedFrames = before.decodedFrames ?? null;
    room.finalDecodedFrames = after.decodedFrames ?? null;
    room.videoWidth = after.videoWidth ?? before.videoWidth ?? null;
    room.videoHeight = after.videoHeight ?? before.videoHeight ?? null;
    room.continuedPlayback = room.playable
      && room.initialCurrentTime !== null
      && room.finalCurrentTime !== null
      && room.finalCurrentTime > room.initialCurrentTime + 0.05
      && room.initialDecodedFrames !== null
      && room.finalDecodedFrames !== null
      && room.finalDecodedFrames > room.initialDecodedFrames;
  });
}

function evaluateProfile(profile) {
  return profile.launchSucceeded
    && profile.isolatedUserData
    && profile.layoutSelected
    && profile.roomIds.length === profile.requestedRoomCount
    && profile.rooms.every((room) => room.playable)
    && profile.rooms.every((room) => room.continuedPlayback)
    && profile.renderer.tileCount === profile.requestedRoomCount
    && profile.renderer.videoCount === profile.requestedRoomCount
    && profile.renderer.playingCount === profile.requestedRoomCount
    && profile.renderer.overlapCount === 0
    && profile.renderer.consoleErrorCount === 0
    && profile.renderer.pageErrorCount === 0
    && profile.processes.sampleCount > 0
    && typeof profile.screenshotFile === 'string'
    && profile.failureCode === undefined;
}

async function runProfile({
  executablePath,
  evidenceDirectory,
  requestedRoomCount,
  roomIds,
  sampleDurationMs,
}) {
  const profile = createProfile(requestedRoomCount, roomIds);
  const userDataDirectory = await mkdtemp(
    join(evidenceDirectory, `user-data-${requestedRoomCount}-`),
  );
  const artifactsDirectory = join(evidenceDirectory, `playwright-${requestedRoomCount}`);
  await mkdir(artifactsDirectory, { recursive: true });

  if (profile.roomIds.length < requestedRoomCount) {
    setFailure(profile, 'configuration', 'INSUFFICIENT_ROOM_IDS');
  }

  const counters = {
    consoleErrorCount: 0,
    consoleWarnCount: 0,
    pageErrorCount: 0,
  };
  const attachedPages = new WeakSet();
  let electronApp;
  let page;

  try {
    electronApp = await _electron.launch({
      executablePath,
      args: [`--user-data-dir=${userDataDirectory}`],
      cwd: projectRoot,
      artifactsDir: artifactsDirectory,
      timeout: WINDOW_TIMEOUT_MS,
    });
    profile.launchSucceeded = true;

    electronApp.on('window', (windowPage) => {
      attachRendererCounters(windowPage, counters, attachedPages);
    });

    const actualUserData = await electronApp.evaluate(({ app }) => app.getPath('userData'));
    profile.isolatedUserData = isPathInside(userDataDirectory, actualUserData);
    if (!profile.isolatedUserData) {
      setFailure(profile, 'user-data', 'USER_DATA_NOT_ISOLATED');
      throw new Error('PROFILE_STOPPED');
    }

    try {
      page = await electronApp.firstWindow({ timeout: WINDOW_TIMEOUT_MS });
      attachRendererCounters(page, counters, attachedPages);
    } catch {
      setFailure(profile, 'window', 'WINDOW_UNAVAILABLE');
      throw new Error('PROFILE_STOPPED');
    }

    for (const room of profile.rooms) {
      const playback = await addRoomThroughUi(page, room.roomId);
      room.playable = playback.playable;
      room.firstFrameMs = playback.firstFrameMs;
      if (!room.playable) {
        setFailure(profile, playback.failureStage, playback.failureCode);
      }
    }

    profile.layoutSelected = await selectProfileLayout(page, requestedRoomCount);
    if (!profile.layoutSelected) {
      setFailure(profile, 'layout', 'LAYOUT_SELECTION_FAILED');
    }

    try {
      const initialPlayback = await readPlaybackMetrics(page, requestedRoomCount);
      const [processes] = await Promise.all([
        sampleProcessMetrics(electronApp, sampleDurationMs),
        delay(sampleDurationMs),
      ]);
      profile.processes = processes;
      const finalPlayback = await readPlaybackMetrics(page, requestedRoomCount);
      applyPlaybackMetrics(profile, initialPlayback, finalPlayback);
    } catch {
      setFailure(profile, 'sampling', 'PROCESS_SAMPLING_FAILED');
    }

    try {
      profile.renderer = await readRendererMetrics(page, counters);
      if (profile.renderer.overlapCount > 0) {
        setFailure(profile, 'renderer', 'ROOM_TILE_OVERLAP');
      }
      if (profile.renderer.consoleErrorCount > 0) {
        setFailure(profile, 'renderer', 'RENDERER_CONSOLE_ERROR');
      }
      if (profile.renderer.pageErrorCount > 0) {
        setFailure(profile, 'renderer', 'RENDERER_PAGE_ERROR');
      }
    } catch {
      setFailure(profile, 'renderer', 'DOM_METRICS_FAILED');
    }
  } catch {
    if (!profile.launchSucceeded) setFailure(profile, 'launch', 'ELECTRON_LAUNCH_FAILED');
    else setFailure(profile, 'profile', 'PROFILE_RUN_FAILED');
  } finally {
    if (page) {
      const screenshotFile = `profile-${requestedRoomCount}.png`;
      try {
        await page.addStyleTag({ content: SCREENSHOT_PRIVACY_STYLE });
        await page.screenshot({ path: join(evidenceDirectory, screenshotFile), fullPage: true });
        profile.screenshotFile = screenshotFile;
      } catch {
        setFailure(profile, 'screenshot', 'SCREENSHOT_FAILED');
      }
    }

    if (electronApp) {
      try {
        await electronApp.close();
      } catch {
        setFailure(profile, 'close', 'ELECTRON_CLOSE_FAILED');
      }
    }
  }

  profile.passed = evaluateProfile(profile);
  return profile;
}

async function main() {
  let roomIds;
  let profileCounts;
  let sampleDurationMs;
  let executablePath;

  try {
    roomIds = parseRoomIds(process.env.DOUYU_PERF_ROOM_IDS);
    profileCounts = parseProfileCounts(process.env.DOUYU_PERF_PROFILES);
    sampleDurationMs = parseSampleDurationMs(process.env.DOUYU_PERF_SAMPLE_MS);
    executablePath = resolveExecutable(process.env.DOUYU_PERF_EXECUTABLE);
  } catch {
    process.stderr.write('PERFORMANCE_CONFIGURATION_INVALID\n');
    process.exitCode = 1;
    return;
  }

  let evidenceDirectory;
  try {
    evidenceDirectory = await mkdtemp(join(tmpdir(), 'douyu-monitor-performance-'));
  } catch {
    process.stderr.write('EVIDENCE_DIRECTORY_CREATE_FAILED\n');
    process.exitCode = 1;
    return;
  }

  const profiles = [];
  for (const requestedRoomCount of profileCounts) {
    try {
      profiles.push(await runProfile({
        executablePath,
        evidenceDirectory,
        requestedRoomCount,
        roomIds,
        sampleDurationMs,
      }));
    } catch {
      const profile = createProfile(requestedRoomCount, roomIds);
      setFailure(profile, 'profile', 'PROFILE_SETUP_FAILED');
      profiles.push(profile);
    }
  }

  const report = {
    schemaVersion: 1,
    generatedAt: new Date().toISOString(),
    executable: basename(executablePath),
    sampleDurationMs,
    profiles,
  };

  try {
    await writeFile(
      join(evidenceDirectory, RESULT_FILE),
      `${JSON.stringify(report, null, 2)}\n`,
      'utf8',
    );
  } catch {
    process.stderr.write('PERFORMANCE_REPORT_WRITE_FAILED\n');
    process.exitCode = 1;
    return;
  }

  process.stdout.write(`Evidence directory under os.tmpdir(): ${basename(evidenceDirectory)}\n`);
  process.stdout.write(`Report file: ${RESULT_FILE}\n`);
  if (profiles.some((profile) => !profile.passed)) {
    process.stderr.write('PERFORMANCE_BASELINE_FAILED\n');
    process.exitCode = 1;
  }
}

await main().catch(() => {
  process.stderr.write('PERFORMANCE_BASELINE_UNEXPECTED_FAILURE\n');
  process.exitCode = 1;
});
