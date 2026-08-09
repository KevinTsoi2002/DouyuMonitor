import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { RoomAvatar } from '../src/renderer/components/RoomAvatar';

describe('RoomAvatar', () => {
  it('renders an image with an accessible anchor name', () => {
    const html = renderToStaticMarkup(
      <RoomAvatar anchorName="示例主播" avatarUrl="https://example.com/avatar.jpg" />,
    );

    expect(html).toContain('src="https://example.com/avatar.jpg"');
    expect(html).toContain('alt="示例主播"');
    expect(html).toContain('referrerPolicy="no-referrer"');
  });

  it('renders initials when no avatar is available', () => {
    const html = renderToStaticMarkup(<RoomAvatar anchorName="示例主播" />);

    expect(html).toContain('示例');
    expect(html).not.toContain('<img');
  });
});
