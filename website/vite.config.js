import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

const pkg = JSON.parse(readFileSync(resolve(process.cwd(), "package.json"), "utf-8"));

export default defineConfig({
  plugins: [react()],
  define: {
    "import.meta.env.VITE_WEB_VERSION": JSON.stringify(pkg.version)
  },
  server: {
    host: true,
    port: 5173
  }
});
