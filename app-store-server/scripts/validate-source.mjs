import { readdir, readFile } from "node:fs/promises";
import { extname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const appsRoot = join(root, "apps");
const forbiddenContent = new Set([
  ".nes",
  ".fds",
  ".nsf",
  ".gb",
  ".gbc",
  ".wad",
  ".iso",
  ".bios",
  ".rom",
]);
const idPattern = /^(?!.*\.\.)[A-Za-z0-9][A-Za-z0-9._-]{2,62}$/;
const semverPattern =
  /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
const artifactPattern = /^[A-Za-z0-9][A-Za-z0-9._-]*\.so$/;

async function walk(directory) {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) files.push(...(await walk(path)));
    else files.push(path);
  }
  return files;
}

const appDirectories = await readdir(appsRoot, { withFileTypes: true });
const artifacts = new Set();
let count = 0;
for (const directory of appDirectories) {
  if (!directory.isDirectory()) continue;
  const manifestPath = join(appsRoot, directory.name, "manifest.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  if (manifest.id !== directory.name || !idPattern.test(manifest.id)) {
    throw new Error(`${directory.name}: manifest id must match its directory`);
  }
  if (!semverPattern.test(manifest.version)) {
    throw new Error(`${manifest.id}: version must be semantic`);
  }
  if (
    manifest.kind !== "app" ||
    manifest.runtime !== "elf" ||
    manifest.entry !== "idreeswatch_module" ||
    manifest.min_os_api !== 1
  ) {
    throw new Error(`${manifest.id}: unsupported reference-app contract`);
  }
  if (
    typeof manifest.artifact !== "string" ||
    !artifactPattern.test(manifest.artifact) ||
    artifacts.has(manifest.artifact)
  ) {
    throw new Error(
      `${manifest.id}: artifact must be a unique, safe .so filename`,
    );
  }
  artifacts.add(manifest.artifact);
  if (
    !manifest.license ||
    typeof manifest.license !== "string" ||
    !manifest.author ||
    !manifest.summary
  ) {
    throw new Error(`${manifest.id}: author, summary, and license are required`);
  }
  for (const file of await walk(join(appsRoot, directory.name))) {
    if (forbiddenContent.has(extname(file).toLowerCase())) {
      throw new Error(`${manifest.id}: copyrighted/runtime content is forbidden: ${file}`);
    }
  }
  count += 1;
}

process.stdout.write(`Validated ${count} app source package(s)\n`);
