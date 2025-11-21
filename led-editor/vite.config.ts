import { defineConfig, type Plugin } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import tailwindcss from "@tailwindcss/vite";
// import { viteSingleFile } from "vite-plugin-singlefile";
import { viteStaticCopy } from "vite-plugin-static-copy";

const viteServerConfig = {
  name: "log-request-middleware",
  configureServer(server) {
    server.middlewares.use((req, res, next) => {
      res.setHeader("Access-Control-Allow-Origin", "*");
      res.setHeader("Access-Control-Allow-Methods", "GET");
      res.setHeader("Cross-Origin-Opener-Policy", "same-origin");
      res.setHeader("Cross-Origin-Embedder-Policy", "require-corp");
      next();
    });
  },
} satisfies Plugin;

// https://vite.dev/config/
export default defineConfig({
  plugins: [
    svelte(),
    tailwindcss(),
    // viteSingleFile(),
    viteServerConfig,
    // viteStaticCopy({
    //   targets: [
    //     {
    //       src: "node_modules/@ffmpeg/core-mt/dist/esm/*",
    //       dest: "wasm-files",
    //     },
    //   ],
    // }),
  ],
  optimizeDeps: {
    // exclude: ["@ffmpeg/ffmpeg", "@ffmpeg/core", "@ffmpeg/core-mt"],
  },
  server: {
    headers: {
      "Cross-Origin-Opener-Policy": "same-origin",
      // "Cross-Origin-Embedder-Policy": "require-corp",
    },
    // fs: {
    //   allow: ["../.."],
    // },
  },
});
