import { readdir, readFile } from "node:fs/promises";
import { extname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const sourceRoots = ["apps", "themes", "watchfaces", "wallpapers", "icon-packs"];
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
const artifactPattern = /^[A-Za-z0-9][A-Za-z0-9._-]*\.(so|json|py|mpy|bin)$/;
const iconPattern = /^[a-z0-9][a-z0-9-]{0,30}$/;

function requireText(manifest, key, maximumBytes) {
  const value = manifest[key];
  if (typeof value !== "string" || value.length === 0 ||
      Buffer.byteLength(value, "utf8") > maximumBytes) {
    throw new Error(`${manifest.id || "package"}.${key} must be 1-${maximumBytes} UTF-8 bytes`);
  }
}

async function walk(directory) {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) files.push(...(await walk(path)));
    else files.push(path);
  }
  return files;
}

let count = 0;
for (const sourceRootName of sourceRoots) {
 const sourceRoot = join(root, sourceRootName);
 let packageDirectories;
 try {
  packageDirectories = await readdir(sourceRoot, { withFileTypes: true });
 } catch (error) {
  if (error.code === "ENOENT") continue;
  throw error;
 }
 for (const directory of packageDirectories) {
  if (!directory.isDirectory()) continue;
  const manifestPath = join(sourceRoot, directory.name, "manifest.json");
  const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
  if (manifest.id !== directory.name || !idPattern.test(manifest.id)) {
    throw new Error(`${directory.name}: manifest id must match its directory`);
  }
  if (!semverPattern.test(manifest.version)) {
    throw new Error(`${manifest.id}: version must be semantic`);
  }
  requireText(manifest, "name", 39);
  requireText(manifest, "version", 19);
  requireText(manifest, "author", 39);
  requireText(manifest, "summary", 111);
  requireText(manifest, "entry", 47);
  const contracts = new Map([
    ["app/elf", "idreeswatch_module"],
    ["theme/content", "theme.palette.v1"],
    ["watchface/content", "watchface.simple.v1"],
    ["wallpaper/content", "wallpaper.rgb565.v1"],
    ["icon-pack/content", "icon-pack.v1"],
    ["app/micropython", "micropython.v1"],
    ["firmware/ota", "firmware.esp32s3.signed.v1"],
  ]);
  if (contracts.get(`${manifest.kind}/${manifest.runtime}`) !== manifest.entry ||
      !Number.isInteger(manifest.min_os_api) || manifest.min_os_api < 1 ||
      manifest.min_os_api > 65535) {
    throw new Error(`${manifest.id}: unsupported package contract`);
  }
  if (manifest.icon !== undefined && !iconPattern.test(manifest.icon)) {
    throw new Error(`${manifest.id}: invalid icon id`);
  }
  if (
    typeof manifest.artifact !== "string" ||
    !artifactPattern.test(manifest.artifact)
  ) {
    throw new Error(
      `${manifest.id}: artifact filename is not supported`,
    );
  }
  if (
    !manifest.license ||
    typeof manifest.license !== "string" ||
    !manifest.author ||
    !manifest.summary
  ) {
    throw new Error(`${manifest.id}: author, summary, and license are required`);
  }
  const packageRoot = join(sourceRoot, directory.name);
  if (manifest.runtime === "content") {
    const payload = await readFile(join(packageRoot, manifest.artifact));
    if (payload.length < 1 || payload.length > 768 * 1024) {
      throw new Error(`${manifest.id}: content payload must be 1 byte through 768 KiB`);
    }
    const content = JSON.parse(payload.toString("utf8"));
    if (content.format !== manifest.entry) {
      throw new Error(`${manifest.id}: payload format must be ${manifest.entry}`);
    }
  }
  for (const file of await walk(packageRoot)) {
    if (forbiddenContent.has(extname(file).toLowerCase())) {
      throw new Error(`${manifest.id}: copyrighted/runtime content is forbidden: ${file}`);
    }
  }
  count += 1;
 }
}

process.stdout.write(`Validated ${count} source package(s)\n`);
