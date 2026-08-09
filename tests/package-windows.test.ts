import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

const packageConfig = JSON.parse(readFileSync('package.json', 'utf8')) as {
  scripts: Record<string, string>;
  build: { electronDist?: string };
  dependencies: Record<string, string>;
  devDependencies: Record<string, string>;
};

describe('Windows packaging configuration', () => {
  it('packages from the Electron runtime already installed in node_modules', () => {
    expect(packageConfig.build.electronDist).toBe('node_modules/electron/dist');
  });

  it('exposes unpacked and NSIS x64 build commands', () => {
    expect(packageConfig.scripts['dist:unpacked']).toContain('electron-builder --win --x64 --dir');
    expect(packageConfig.scripts['dist:win']).toContain('electron-builder --win --x64');
  });

  it('exposes the packaged Electron performance baseline command', () => {
    expect(packageConfig.scripts['test:performance']).toBe(
      'node scripts/performance-baseline.mjs',
    );
  });

  it('keeps build tooling out of packaged production dependencies', () => {
    for (const dependency of ['@vitejs/plugin-react', 'typescript', 'vite', 'vitest']) {
      expect(packageConfig.dependencies).not.toHaveProperty(dependency);
      expect(packageConfig.devDependencies).toHaveProperty(dependency);
    }
    expect(packageConfig.dependencies).toHaveProperty('ws');
  });
});
