import { readdir, readFile } from "node:fs/promises";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const appsRoot = join(root, "apps");
const directories = await readdir(appsRoot, { withFileTypes: true });
const include = [];

for (const directory of directories) {
  if (!directory.isDirectory()) continue;
  const manifestPath = join(appsRoot, directory.name, "manifest.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  if (manifest.runtime !== "elf") {
    // Only native ELF apps need an ESP-IDF shared-object build. MicroPython
    // and content payloads ship from source and are staged during deploy.
    continue;
  }
  include.push({
    id: manifest.id,
    path: `app-store-server/apps/${directory.name}`,
    manifest: `app-store-server/apps/${directory.name}/manifest.json`,
    artifact: manifest.artifact,
  });
}

include.sort((left, right) => left.id.localeCompare(right.id));
if (include.length === 0) throw new Error("No source apps found");
process.stdout.write(`matrix=${JSON.stringify({ include })}\n`);
