import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// outDir 'dist' is ui/dist, which is where tauri.conf.json's frontendDist ("../ui/dist", relative
// to src-tauri/) resolves to. The two must agree or the binary embeds nothing and shows a blank
// window.
//
// base './' matters more than it looks: the bundle is served from the tauri:// origin, not a web
// server root, so absolute asset paths resolve to nothing and the window comes up blank.
export default defineConfig({
  plugins: [react()],
  base: './',
  build: { outDir: 'dist', emptyOutDir: true },
  server: { port: 5173, strictPort: true },
})
