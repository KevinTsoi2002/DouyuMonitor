import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import rendererConfig from '../vite.config';

describe('renderer build config', () => {
  it('uses relative assets so the Electron file target can load the bundle', () => {
    expect(rendererConfig.base).toBe('./');
  });

  it('declares a renderer CSP without unsafe script execution', () => {
    const html = readFileSync(new URL('../index.html', import.meta.url), 'utf8');

    expect(html).toContain('http-equiv="Content-Security-Policy"');
    expect(html).toContain("script-src 'self'");
    expect(html).toContain("connect-src 'self' http: https: ws: wss:");
    expect(html).not.toContain("script-src 'self' 'unsafe-eval'");
  });

  it('keeps the tile top bar as a horizontal menu anchor', () => {
    const css = readFileSync(new URL('../src/renderer/styles.css', import.meta.url), 'utf8');

    expect(css).toMatch(/\.tile-topbar\s*\{[^}]*display:\s*flex/);
  });

  it('anchors the danmaku settings panel to the mobile viewport', () => {
    const css = readFileSync(new URL('../src/renderer/styles.css', import.meta.url), 'utf8');

    expect(css).toMatch(
      /@media \(max-width: 820px\)[\s\S]*?\.danmaku-settings-panel\s*\{[^}]*position:\s*fixed[^}]*left:\s*12px[^}]*right:\s*12px/,
    );
  });
});
