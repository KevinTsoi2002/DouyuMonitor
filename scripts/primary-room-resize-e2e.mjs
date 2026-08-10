import { spawn } from 'node:child_process';
import { createServer } from 'node:net';
import { resolve } from 'node:path';
import { chromium } from 'playwright';

const projectRoot = resolve(import.meta.dirname, '..');

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

async function reservePort() {
  const server = createServer();
  await new Promise((resolveListen, rejectListen) => {
    server.once('error', rejectListen);
    server.listen(0, '127.0.0.1', resolveListen);
  });
  const address = server.address();
  const port = typeof address === 'object' && address ? address.port : 0;
  await new Promise((resolveClose) => server.close(resolveClose));
  return port;
}

async function waitForServer(url) {
  const deadline = Date.now() + 20_000;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) return;
    } catch {
      await new Promise((resolveDelay) => setTimeout(resolveDelay, 150));
    }
  }
  throw new Error('VITE_SERVER_TIMEOUT');
}

async function addRoom(page, roomId) {
  await page.locator('button[aria-label="添加直播间"]').first().click();
  await page.locator('#room-search').fill(roomId);
  await page.locator('.room-search-form button[type="submit"]').click();
  await page.locator('.search-result').filter({ hasText: roomId }).first().click();
  await page.locator(`.room-tile[data-room-id="${roomId}"]`).waitFor({ state: 'visible' });
}

async function overlapCount(page) {
  return page.locator('.room-tile').evaluateAll((tiles) => {
    const rectangles = tiles.map((tile) => tile.getBoundingClientRect());
    let overlaps = 0;
    for (let leftIndex = 0; leftIndex < rectangles.length; leftIndex += 1) {
      for (let rightIndex = leftIndex + 1; rightIndex < rectangles.length; rightIndex += 1) {
        const left = rectangles[leftIndex];
        const right = rectangles[rightIndex];
        if (left.left < right.right && left.right > right.left && left.top < right.bottom && left.bottom > right.top) {
          overlaps += 1;
        }
      }
    }
    return overlaps;
  });
}

const port = await reservePort();
const url = `http://127.0.0.1:${port}`;
const viteProcess = spawn(
  process.execPath,
  [resolve(projectRoot, 'node_modules/vite/bin/vite.js'), '--host', '127.0.0.1', '--port', String(port), '--strictPort'],
  { cwd: projectRoot, stdio: 'ignore' },
);
let browser;

try {
  await waitForServer(url);
  browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const consoleErrors = [];
  page.on('console', (message) => {
    if (message.type() === 'error') consoleErrors.push(message.text());
  });
  await page.addInitScript(() => localStorage.clear());
  await page.goto(url);
  assert(await page.locator('.primary-room-divider').count() === 0, 'DIVIDER_VISIBLE_OUTSIDE_PRIMARY_LAYOUT');
  await page.locator('.layout-menu-trigger').click();
  await page.locator('.layout-option').filter({ hasText: '主画面布局' }).click();

  const divider = page.locator('.primary-room-divider');
  await divider.waitFor({ state: 'visible' });
  const gridBounds = await page.locator('.workspace-grid').boundingBox();
  const dividerBounds = await divider.boundingBox();
  assert(gridBounds && dividerBounds, 'RESIZE_BOUNDS_UNAVAILABLE');
  await page.mouse.move(dividerBounds.x + dividerBounds.width / 2, dividerBounds.y + dividerBounds.height / 2);
  await page.mouse.down();
  await page.mouse.move(gridBounds.x + gridBounds.width * 0.64, dividerBounds.y + dividerBounds.height / 2, { steps: 8 });
  await page.waitForTimeout(3_200);
  assert(
    await page.locator('.room-tile.is-primary').evaluate((tile) => tile.classList.contains('controls-visible')),
    'PRIMARY_CONTROLS_HIDDEN_DURING_RESIZE',
  );
  await page.mouse.up();
  assert(await divider.getAttribute('aria-valuenow') === '67', 'RESIZE_DID_NOT_SNAP_TO_67');

  await divider.focus();
  await page.keyboard.press('Home');
  assert(await divider.getAttribute('aria-valuenow') === '50', 'HOME_DID_NOT_SELECT_MINIMUM');
  await page.keyboard.press('End');
  assert(await divider.getAttribute('aria-valuenow') === '67', 'END_DID_NOT_SELECT_MAXIMUM');
  await divider.dblclick();
  assert(await divider.getAttribute('aria-valuenow') === '60', 'DOUBLE_CLICK_DID_NOT_RESET');
  await page.mouse.move(dividerBounds.x + dividerBounds.width / 2, dividerBounds.y + dividerBounds.height / 2);
  await page.mouse.down();
  await page.mouse.move(gridBounds.x + gridBounds.width * 0.5, dividerBounds.y + dividerBounds.height / 2, { steps: 4 });
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert(await divider.getAttribute('aria-valuenow') === '60', 'ESCAPE_DID_NOT_CANCEL_PREVIEW');

  await page.evaluate(() => {
    globalThis.__primaryResizeNodes = new Map(
      [...document.querySelectorAll('.room-tile')].map((tile) => [tile.getAttribute('data-room-id'), {
        tile,
        playback: tile.querySelector('.signal-scene, .live-video-surface, .playback-state-surface'),
        danmaku: tile.querySelector('.danmaku-overlay'),
      }]),
    );
  });
  const targetTile = page.locator('.room-tile[data-room-id="270888"]');
  await targetTile.hover();
  await targetTile.getByRole('button', { name: '设 林深 为主画面' }).click();
  await page.locator('.room-tile[data-room-id="270888"].is-primary').waitFor({ state: 'visible' });
  const nodesPreserved = await page.evaluate(() => (
    [...document.querySelectorAll('.room-tile')].every((tile) => {
      const previous = globalThis.__primaryResizeNodes.get(tile.getAttribute('data-room-id'));
      return previous?.tile === tile
        && previous.playback === tile.querySelector('.signal-scene, .live-video-surface, .playback-state-surface')
        && previous.danmaku === tile.querySelector('.danmaku-overlay');
    })
  ));
  assert(nodesPreserved, 'ROOM_TILE_NODE_REPLACED');
  assert(await overlapCount(page) === 0, 'DESKTOP_TILE_OVERLAP');

  const thirdTile = page.locator('.room-tile[data-room-id="385729"]');
  await thirdTile.hover();
  await thirdTile.getByRole('button', { name: '白昼 更多操作' }).click();
  await thirdTile.getByRole('menuitem', { name: '移除房间' }).click();
  await page.waitForFunction(() => document.querySelectorAll('.room-tile').length === 2);
  assert(await overlapCount(page) === 0, 'TWO_ROOM_TILE_OVERLAP');

  const firstTile = page.locator('.room-tile[data-room-id="63136"]');
  await firstTile.hover();
  await firstTile.getByRole('button', { name: '星河 更多操作' }).click();
  await firstTile.getByRole('menuitem', { name: '移除房间' }).click();
  await page.waitForFunction(() => document.querySelectorAll('.room-tile').length === 1);
  assert(await page.locator('.primary-room-divider').count() === 0, 'SINGLE_ROOM_DIVIDER_VISIBLE');

  await page.setViewportSize({ width: 1920, height: 1080 });
  for (const roomId of ['63136', '385729', '4', '5', '6', '7', '8', '9']) {
    await addRoom(page, roomId);
  }
  assert(await page.locator('.room-tile').count() === 9, 'NINE_ROOM_COUNT_MISMATCH');
  assert(await overlapCount(page) === 0, 'NINE_ROOM_TILE_OVERLAP');

  await page.setViewportSize({ width: 390, height: 844 });
  await page.waitForFunction(() => (
    document.querySelector('.primary-room-divider')?.getAttribute('aria-orientation') === 'horizontal'
  ));
  assert(await overlapCount(page) === 0, 'NARROW_TILE_OVERLAP');
  assert(consoleErrors.length === 0, `CONSOLE_ERRORS:${consoleErrors.join('|')}`);
  process.stdout.write('PRIMARY_ROOM_RESIZE_E2E_PASS\n');
} finally {
  if (browser) await browser.close();
  viteProcess.kill();
}
