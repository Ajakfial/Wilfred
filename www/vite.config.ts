import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Project Pages site serves from https://<user>.github.io/Wilfred/
export default defineConfig({
  base: '/Wilfred/',
  plugins: [react()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
  },
});
