import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { DEFAULT_DANMAKU_SETTINGS } from '../src/renderer/danmaku/danmaku-settings';
import {
  DanmakuLines,
  type LaunchedDanmaku,
} from '../src/renderer/components/DanmakuOverlay';

function launched(index: number): LaunchedDanmaku {
  return {
    message: {
      id: String(index),
      roomId: '63136',
      nickname: `User ${index}`,
      text: index === 0 ? '<script>bad()</script>' : `Message ${index}`,
      receivedAt: '2026-08-07T00:00:00.000Z',
    },
    laneIndex: index,
    top: index * 32.4,
    width: 180,
    containerWidth: 800,
    launchedAt: 1000 + index * 80,
    durationMs: 8000,
    fontSize: 24,
  };
}

describe('DanmakuLines', () => {
  it('renders launched messages as escaped positioned scrolling text', () => {
    const html = renderToStaticMarkup(
      <DanmakuLines
        messages={[launched(0)]}
        settings={DEFAULT_DANMAKU_SETTINGS}
        onExpire={() => {}}
      />,
    );

    expect(html).toContain('class="danmaku-line danmaku-rendering-native"');
    expect(html).toContain('--danmaku-top:0px');
    expect(html).toContain('--danmaku-duration:8000ms');
    expect(html).toContain('--danmaku-travel:-980px');
    expect(html).toContain('User 0：');
    expect(html).toContain('&lt;script&gt;bad()&lt;/script&gt;');
    expect(html).not.toContain('<script>');
    expect(html).toContain('aria-label="弹幕"');
  });

  it('renders more than five simultaneous messages without truncation', () => {
    const html = renderToStaticMarkup(
      <DanmakuLines
        messages={Array.from({ length: 12 }, (_, index) => launched(index))}
        settings={{ ...DEFAULT_DANMAKU_SETTINGS, rendering: 'advanced' }}
        onExpire={() => {}}
      />,
    );

    expect(html.match(/class="danmaku-line danmaku-rendering-advanced"/g)).toHaveLength(12);
    expect(html).toContain('Message 11');
  });
});
