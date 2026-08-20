import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    target: 'node22',
    outDir: 'dist/main',
    emptyOutDir: true,
    lib: {
      entry: 'src/main/main.ts',
      formats: ['es'],
      fileName: () => 'main.js',
    },
    rollupOptions: {
      external: [
        'electron',
        'node:buffer',
        'node:child_process',
        'node:crypto',
        'node:fs',
        'node:http',
        'node:https',
        'node:path',
        'node:url',
        'node:util',
        'ws',
      ],
    },
  },
});
