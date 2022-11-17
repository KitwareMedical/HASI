import { defineConfig } from "vite";

export default defineConfig({
  build: {
    lib: {
      entry: "src/hasi-app.ts",
      formats: ["es"],
    },
    rollupOptions: {
      external: /^lit/,
    },
  },
});
