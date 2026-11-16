import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { defineConfig } from "vitest/config";

// The test saves, by name, in base64. Vitest hands them to the tests, which get them with test/saves.ts. The tests of
// the Worker itself run in Cloudflare's runtime, which can't read files, so this is how every test gets them.
const saves: Record<string, string> = {};
const savesDir = join(import.meta.dirname, "test", "saves");
for (const file of readdirSync(savesDir))
  if (file.endsWith(".omwsave")) saves[file.replace(/\.omwsave$/, "")] = readFileSync(join(savesDir, file)).toString("base64");

export default defineConfig({
  test: {
    projects: [
      {
        // Tests of plain functions.
        test: { name: "unit", include: ["test/unit/**/*.test.ts"], environment: "node", provide: { saves } },
      },
    ],
  },
});
