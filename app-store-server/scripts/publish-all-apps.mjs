import { execFileSync } from "node:child_process";
import { readdir, readFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const [artifactsArgument] = process.argv.slice(2);
if (!artifactsArgument) {
  throw new Error("usage: node scripts/publish-all-apps.mjs <artifacts-dir>");
}

const root = fileURLToPath(new URL("..", import.meta.url));
const publisher = join(root, "scripts", "publish-built-app.mjs");
const artifactsRoot = resolve(artifactsArgument);
for (const sourceRootName of ["apps", "themes", "watchfaces"]) {
  const sourceRoot = join(root, sourceRootName);
  let directories;
  try {
    directories = await readdir(sourceRoot, { withFileTypes: true });
  } catch (error) {
    if (error.code === "ENOENT") continue;
    throw error;
  }
  for (const directory of directories) {
    if (!directory.isDirectory()) continue;
    const packageRoot = join(sourceRoot, directory.name);
    const manifestPath = join(packageRoot, "manifest.json");
    const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
    const payloadPath =
      manifest.runtime === "content" || manifest.runtime === "micropython"
        ? join(packageRoot, manifest.artifact)
        : join(artifactsRoot, manifest.artifact);
    execFileSync(
      process.execPath,
      [publisher, manifestPath, payloadPath],
      { stdio: "inherit" },
    );
  }
}
