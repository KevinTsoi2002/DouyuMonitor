async (page) => {
  // Paste this function into Playwright MCP browser_run_code. It uses the existing page.
  const base = 'http://127.0.0.1:8899/';
  const keys = ['cstg-review-theme', 'cstg-visual-review-r3-preference'];
  const assertions = [];
  const runtimeErrors = [];
  const originalViewport = page.viewportSize();
  let originalStorage = null;
  let videoReference = null;
  let phase = 'capture original preferences';
  let failure = null;
  let restoration = {storage: false, reloaded: false, viewport: false};
  const onPageError = error => runtimeErrors.push(error.message);
  const check = (condition, name, evidence = null) => {
    assertions.push({name, passed: Boolean(condition), evidence});
    if (!condition) throw new Error(name);
  };
  const dialog = page.locator('#review-dialog');
  const titlebarTools = page.locator('#watch-tools-trigger');
  const focusedId = () => page.evaluate(() => document.activeElement?.id || '');
  const isImmersive = () => page.locator('body').evaluate(el => el.classList.contains('immersive'));
  const captureVideoNodes = async () => {
    if (videoReference) await videoReference.dispose();
    videoReference = await page.evaluateHandle(() => ({
      grid: document.querySelector('.video-grid'),
      tiles: [...document.querySelectorAll('.video-tile')],
      scenes: [...document.querySelectorAll('.video-scene')],
      pictures: [...document.querySelectorAll('.video-scene > svg')]
    }));
  };
  const checkVideoNodes = async name => {
    const identity = await videoReference.evaluate(saved => {
      const same = (old, selector) => {
        const current = [...document.querySelectorAll(selector)];
        return current.length === old.length && old.every((node, index) => node.isConnected && node === current[index]);
      };
      return {
        grid: saved.grid?.isConnected && saved.grid === document.querySelector('.video-grid'),
        tiles: same(saved.tiles, '.video-tile'),
        scenes: same(saved.scenes, '.video-scene'),
        pictures: same(saved.pictures, '.video-scene > svg')
      };
    });
    check(Object.values(identity).every(Boolean), name, identity);
  };
  const openTools = async () => {
    const before = await page.locator('.video-grid').boundingBox();
    await titlebarTools.click();
    await dialog.waitFor({state: 'visible'});
    check(await page.locator('#dialog-title').innerText() === '观看工具', phase + ': tools opened');
    check(await dialog.getAttribute('data-presentation') === 'drawer', phase + ': tools use drawer');
    const after = await page.locator('.video-grid').boundingBox();
    check(['x','y','width','height'].every(key => Math.abs(before[key]-after[key])<=1), phase + ': drawer does not resize or move video grid', {before,after});
  };
  const closeToTitlebar = async () => {
    await page.keyboard.press('Escape');
    await dialog.waitFor({state: 'hidden'});
    await page.waitForFunction(() => document.activeElement?.id === 'watch-tools-trigger', null, {timeout: 3000});
    check(await focusedId() === 'watch-tools-trigger', phase + ': focus returns to titlebar');
    await page.waitForFunction(() => document.getElementById('watch-tools-trigger').getAttribute('aria-expanded') === 'false', null, {timeout: 3000});
    check(await titlebarTools.getAttribute('aria-expanded') === 'false', phase + ': trigger collapsed');
  };
  const enterImmersive = async () => {
    await openTools();
    await dialog.locator('[data-action="immersive"]').click();
    await dialog.waitFor({state: 'hidden'});
    await page.mouse.move(20, 730);
    check(await isImmersive(), phase + ': immersive enabled');
    check(await page.locator('.watch-stage').evaluate(el => el === document.activeElement), phase + ': stage receives focus');
    check(await page.locator('.edge-panel').isHidden(), phase + ': idle edge panel hidden');
  };
  const restoreStorage = values => {
    for (const [key, value] of Object.entries(values)) {
      if (value === null) localStorage.removeItem(key);
      else localStorage.setItem(key, value);
    }
  };

  try {
    // A static same-origin asset lets us capture storage before the app writes its default theme.
    if (new URL(page.url()).origin !== new URL(base).origin) {
      await page.goto(base + 'assets/icon-app.png');
    }
    originalStorage = await page.evaluate(names => Object.fromEntries(names.map(key => [key, localStorage.getItem(key)])), keys);
    page.on('pageerror', onPageError);
    await page.setViewportSize({width: 1366, height: 768});
    await page.goto(base);
    await titlebarTools.waitFor({state: 'visible'});
    check(await page.locator('body').getAttribute('data-review') === 'r3', 'R3 page loaded');
    check(await page.locator('.video-tile').count() === 4, 'four video composition placeholders');
    check(await page.locator('.scene-caption,.watch-dock,.watch-watermark,.immersion-exit').count() === 0, 'old permanent picture overlays removed');
    check(await titlebarTools.evaluate(el => Boolean(el.closest('.titlebar'))), 'tools trigger lives in existing titlebar');
    await captureVideoNodes();

    phase = 'rooms drawer';
    await openTools();
    check(await titlebarTools.getAttribute('aria-expanded') === 'true', 'titlebar trigger expanded');
    await checkVideoNodes('opening tools preserves grid and picture nodes');
    await dialog.locator('[data-action="rooms"]').click();
    check(await page.locator('#dialog-title').innerText() === '房间', 'rooms subdrawer opened');
    check(await dialog.getAttribute('data-presentation') === 'drawer', 'rooms retains drawer presentation');
    check(await dialog.locator('.watch-room').count() === 4, 'rooms drawer lists all four initial rooms');
    await checkVideoNodes('rooms subdrawer preserves grid and picture nodes');
    await closeToTitlebar();
    await checkVideoNodes('closing rooms preserves grid and picture nodes');

    phase = 'layout drawer';
    await openTools();
    await dialog.locator('[data-action="layout"]').click();
    check(await page.locator('#dialog-title').innerText() === '观看布局', 'layout subdrawer opened');
    await checkVideoNodes('opening layout preserves grid and picture nodes');
    await dialog.locator('[data-layout-choice="等分网格"]').click();
    await dialog.waitFor({state: 'hidden'});
    check(await page.locator('.video-grid').getAttribute('data-layout') === '等分网格', 'layout choice applied');
    await page.waitForFunction(() => document.activeElement?.id === 'watch-tools-trigger', null, {timeout: 3000});
    check(await focusedId() === 'watch-tools-trigger', 'layout selection restores titlebar focus');
    await checkVideoNodes('changing layout preserves grid and picture nodes');

    phase = 'audio drawer';
    await openTools();
    await dialog.locator('[data-action="audio"]').click();
    check(await page.locator('#dialog-title').innerText() === '音频与弹幕', 'audio subdrawer opened');
    await dialog.locator('[data-audio="muted"]').check();
    await dialog.locator('[data-audio="danmaku"]').uncheck();
    check(await dialog.locator('[data-audio="muted"]').isChecked(), 'audio mute checked');
    check(!(await dialog.locator('[data-audio="danmaku"]').isChecked()), 'danmaku unchecked');
    await checkVideoNodes('audio changes preserve grid and picture nodes');
    await closeToTitlebar();
    check(await page.locator('.danmaku-example').isHidden(), 'danmaku overlay hidden after change');
    await page.locator('.main-video').hover();
    check(await page.locator('.main-video .video-heading small').innerText() === '主画面 · 全局静音', 'mute feedback visible on hover');

    phase = 'day and night switching';
    for (const palette of ['c3', 'c1', 'c3']) {
      await page.locator(`.scheme-choice[data-palette="${palette}"]`).click();
      check(await page.locator('body').getAttribute('data-palette') === palette, 'palette applied ' + palette);
      check(await page.locator('body').getAttribute('data-scheme') === 'c', 'C component structure retained ' + palette);
      check(await page.locator('.video-grid').getAttribute('data-layout') === '等分网格', 'layout retained ' + palette);
      check(await page.locator('.danmaku-example').isHidden(), 'danmaku retained ' + palette);
      check(await page.locator('.video-controls [data-action="mute"]').evaluateAll(nodes => nodes.length === 4 && nodes.every(node => node.getAttribute('aria-pressed') === 'true')), 'mute retained ' + palette);
      check(await page.evaluate(() => localStorage.getItem('cstg-review-theme')) === palette, 'theme persisted ' + palette);
      await checkVideoNodes('palette preserves grid and picture nodes ' + palette);
    }

    phase = 'dialog text input and fifth room';
    await openTools();
    await dialog.locator('[data-action="add"]').click();
    check(await dialog.getAttribute('data-presentation') === 'modal', 'add flow changes drawer to modal');
    await page.locator('#add-query').fill('');
    await page.locator('#add-query').press('t');
    check(await page.locator('#add-query').inputValue() === 't', 'T is entered in dialog input');
    check(await page.locator('#dialog-title').innerText() === '添加房间', 'T in dialog input does not replace dialog with tools');
    await checkVideoNodes('opening add and typing preserve grid and picture nodes');
    await page.locator('#add-query').fill('迟夏');
    await dialog.locator('[data-action="search-room"]').click();
    await dialog.locator('[data-join="7"]').click();
    await dialog.waitFor({state: 'hidden'});
    check(await page.locator('#nav-room-count').innerText() === '5', 'fifth room added');
    check(await page.locator('.video-tile').count() === 4, 'fifth room keeps four composition previews');
    // Joining changes membership and may render the page; establish a new reference afterwards.
    await captureVideoNodes();
    await openTools();
    await dialog.locator('[data-action="rooms"]').click();
    check(await dialog.locator('.watch-room').count() === 5, 'fifth room remains in room drawer');
    const fifthRoom = dialog.locator('.watch-room').filter({hasText: '迟夏'});
    check(await fifthRoom.isVisible(), 'fifth room is visible');
    check((await fifthRoom.innerText()).includes('未展示构图'), 'fifth room explicitly marked outside composition');
    await closeToTitlebar();
    await checkVideoNodes('fifth-room drawer operations preserve current picture nodes');

    phase = 'immersive entry and Escape order';
    await enterImmersive();
    await checkVideoNodes('entering immersive preserves grid and picture nodes');
    await page.keyboard.press('t');
    await dialog.waitFor({state: 'visible'});
    check(await page.locator('#dialog-title').innerText() === '观看工具', 'T opens tools in immersive mode');
    check(await isImmersive(), 'T leaves immersive mode active');
    await page.keyboard.press('Escape');
    await dialog.waitFor({state: 'hidden'});
    check(await isImmersive(), 'first Escape only closes tools');
    check(await page.locator('.watch-stage').evaluate(el => el === document.activeElement), 'keyboard tools close returns to stage');
    await page.keyboard.press('Escape');
    check(!(await isImmersive()), 'second Escape exits immersive');
    check(await focusedId() === 'watch-tools-trigger', 'immersive exit returns focus to titlebar trigger');
    check(await page.locator('#immersive-edge').isHidden(), 'edge unavailable outside immersive mode');
    await checkVideoNodes('Escape sequence preserves grid and picture nodes');

    phase = 'immersive edge hover and keyboard focus';
    await enterImmersive();
    const edge = page.locator('#immersive-edge');
    const edgePanel = page.locator('.edge-panel');
    const edgeRect = await edge.boundingBox();
    check(Boolean(edgeRect && edgeRect.width > 0 && edgeRect.height > 0), 'idle edge has a hit area');
    await page.mouse.move(edgeRect.x + edgeRect.width / 2, edgeRect.y + Math.min(4, edgeRect.height / 2));
    await edgePanel.waitFor({state: 'visible'});
    check(await edgePanel.isVisible(), 'hover reveals edge panel');
    await page.mouse.move(20, 730);
    await edgePanel.waitFor({state: 'hidden'});
    check(await edgePanel.isHidden(), 'leaving unfocused edge hides panel');
    await edge.focus();
    check(await focusedId() === 'immersive-edge' && await edgePanel.isVisible(), 'edge keyboard focus reveals panel');
    await page.keyboard.press('Tab');
    check(await focusedId() === 'edge-tools-trigger', 'Tab enters revealed tools button');
    await page.mouse.move(30, 720);
    check(await edgePanel.isVisible(), 'pointer departure does not hide focused controls');
    await page.keyboard.press('Enter');
    await dialog.waitFor({state: 'visible'});
    check(await page.locator('#dialog-title').innerText() === '观看工具', 'edge button opens tools with Enter');
    await page.keyboard.press('Escape');
    await dialog.waitFor({state: 'hidden'});
    await page.waitForFunction(() => document.activeElement?.id === 'edge-tools-trigger', null, {timeout: 3000});
    check(await focusedId() === 'edge-tools-trigger' && await edgePanel.isVisible(), 'closing edge tools returns to visible focused trigger');
    await page.keyboard.press('Escape');
    check(!(await isImmersive()) && await focusedId() === 'watch-tools-trigger', 'edge flow exits to titlebar trigger');
    await checkVideoNodes('edge interaction preserves grid and picture nodes');

    phase = 'preference and theme persistence';
    await page.locator('.scheme-choice[data-palette="c3"]').click();
    await page.locator('#record-choice').click();
    const testNote = 'R3 automated interaction check; original preference restored in finally.';
    await page.locator('#preference-note').fill(testNote);
    await dialog.locator('[data-action="save-preference"]').click();
    await dialog.waitFor({state: 'hidden'});
    const stored = await page.evaluate(() => JSON.parse(localStorage.getItem('cstg-visual-review-r3-preference')));
    check(stored?.revision === 'r3' && stored.components === 'C' && stored.scheme === 'c3' && stored.themes?.night === 'c1' && stored.themes?.day === 'c3' && stored.note === testNote, 'R3 preference saved with confirmed day/night themes');
    await page.reload();
    await titlebarTools.waitFor({state: 'visible'});
    check(await page.locator('body').getAttribute('data-palette') === 'c3', 'day theme restored after reload');
    check((await page.locator('#saved-preference').innerText()).includes('布局意见已本地保存'), 'saved layout feedback restored after reload');
    await page.locator('#record-choice').click();
    check(await page.locator('#preference-note').inputValue() === testNote, 'saved layout note restored after reload');
    await page.keyboard.press('Escape');
    await page.locator('.scheme-choice[data-palette="c1"]').click();
    await page.reload();
    await titlebarTools.waitFor({state: 'visible'});
    check(await page.locator('body').getAttribute('data-palette') === 'c1', 'night theme restored after reload');
  } catch (error) {
    failure = {phase, message: error.message};
    if (!assertions.some(item => !item.passed)) assertions.push({name: phase, passed: false, evidence: error.message});
  } finally {
    if (videoReference) await videoReference.dispose().catch(() => {});
    if (originalStorage !== null) {
      try {
        await page.evaluate(restoreStorage, originalStorage);
        await page.reload();
        await titlebarTools.waitFor({state: 'visible'});
        restoration.reloaded = true;
        // Startup writes the default theme. Reapply null/invalid originals after reload as well.
        await page.evaluate(restoreStorage, originalStorage);
        restoration.storage = await page.evaluate(values => Object.entries(values).every(([key, value]) => localStorage.getItem(key) === value), originalStorage);
      } catch (error) {
        restoration.error = error.message;
        // If reload failed, still attempt to restore the keys on the current same-origin document.
        await page.evaluate(restoreStorage, originalStorage).catch(() => {});
      }
    }
    try {
      if (originalViewport) await page.setViewportSize(originalViewport);
      restoration.viewport = true;
    } catch (error) {
      restoration.viewportError = error.message;
    }
    page.off('pageerror', onPageError);
    assertions.push({name: 'original preferences restored and app reloaded', passed: restoration.storage && restoration.reloaded, evidence: restoration});
    assertions.push({name: 'original viewport restored', passed: restoration.viewport, evidence: restoration.viewportError || null});
    assertions.push({name: 'no runtime JavaScript errors', passed: runtimeErrors.length === 0, evidence: runtimeErrors});
  }
  return {
    revision: 'r3', date: new Date().toISOString(), url: base, viewport: {width: 1366, height: 768},
    passed: !failure && assertions.every(item => item.passed),
    assertionCount: assertions.length, assertions, failure, runtimeErrors, restoration,
    limits: ['DOM picture placeholders only; no real playback', 'Touch, responsive matrix and screenshots are separate checks']
  };
}
