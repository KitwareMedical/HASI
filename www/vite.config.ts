import { defineConfig } from "vite";

export default defineConfig({
  resolve: {
    alias: {
      "~@lumino": "node_modules/@lumino",
    },
  },
});
