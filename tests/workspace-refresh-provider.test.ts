import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

describe('WorkspaceProvider refresh wiring', () => {
  it('owns the production scheduler and disposes it with the Provider', () => {
    const source = readFileSync(
      new URL('../src/renderer/store/workspace-context.tsx', import.meta.url),
      'utf8',
    );

    expect(source).toContain('createWorkspaceRefreshScheduler');
    expect(source).toContain('refreshRoomMetadata');
    expect(source).toContain('scheduler.sync()');
    expect(source).toContain('scheduler.dispose');
    expect(source).toContain('if (demoMode) return');
  });
});
