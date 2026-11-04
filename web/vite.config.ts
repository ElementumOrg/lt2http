import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The build goes into dist/, which CMake builds into the lt2http binary, served at /web/.
export default defineConfig({
  base: '/web/',
  plugins: [react()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    // React and Mantine are one bundle, it is served from the binary, not over the internet
    chunkSizeWarningLimit: 1024,
    // No hashes in file names: lt2http serves them without caching, and dist/ diffs stay readable
    rollupOptions: {
      output: {
        entryFileNames: 'assets/[name].js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name][extname]',
      },
    },
  },
  // npm run dev: the API of an lt2http running on the default port
  server: {
    proxy: {
      '^/(torrents|service|settings|languages|status|info|changelog)': 'http://localhost:65225',
    },
  },
});
