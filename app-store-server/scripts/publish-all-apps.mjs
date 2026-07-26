import { execFileSync } from "node:child_process";
import { readdir, readFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const [artifactsArgument] = process.argv.slice(2);
if (!artifactsArgument) {
  throw new Error("usage: node scripts/publish-all-apps.mjs <artifacts-dir>");
}

const root = fileURLToPath(new URL("..", import.meta.url));
const appsRoot = join(root, "apps");
const publisher = join(root, "scripts", "publish-built-app.mjs");
const artifactsRoot = resolve(artifactsArgument);
const directories = await readdir(appsRoot, { withFileTypes: true });

for (const directory of directories) {
  if (!directory.isDirectory()) continue;
  const manifestPath = join(appsRoot, directory.name, "manifest.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  execFileSync(
    process.execPath,
    [publisher, manifestPath, join(artifactsRoot, manifest.artifact)],
    { stdio: "inherit" },
  );
}
