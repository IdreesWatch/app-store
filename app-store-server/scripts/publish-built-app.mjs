import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { basename, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const [manifestArgument, payloadArgument] = process.argv.slice(2);
if (!manifestArgument || !payloadArgument) {
  throw new Error(
    "usage: node scripts/publish-built-app.mjs <manifest.json> <payload>",
  );
}

const root = fileURLToPath(new URL("..", import.meta.url));
const manifestPath = resolve(manifestArgument);
const payloadPath = resolve(payloadArgument);
const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
const payload = await readFile(payloadPath);

const idPattern = /^(?!.*\.\.)[A-Za-z0-9][A-Za-z0-9._-]{2,62}$/;
const semverPattern =
  /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
const iconPattern = /^[a-z0-9][a-z0-9-]{0,30}$/;
const textWithin = (value, maximumBytes) =>
  typeof value === "string" && value.length > 0 &&
  Buffer.byteLength(value, "utf8") <= maximumBytes;
if (!idPattern.test(manifest.id) || !semverPattern.test(manifest.version)) {
  throw new Error("Manifest has an unsafe id or invalid semantic version");
}
if (!Number.isInteger(manifest.min_os_api) || manifest.min_os_api < 1 ||
    manifest.min_os_api > 65535 || !textWithin(manifest.name, 39) ||
    !textWithin(manifest.version, 19) || !textWithin(manifest.author, 39) ||
    !textWithin(manifest.summary, 111) || !textWithin(manifest.entry, 47)) {
  throw new Error("Manifest metadata is incomplete or outside firmware limits");
}
if (manifest.icon !== undefined && !iconPattern.test(manifest.icon)) {
  throw new Error("Manifest icon id is invalid");
}
if (payload.length < 1 || payload.length > 32 * 1024 * 1024) {
  throw new Error("Package payload must be 1 byte through 32 MiB");
}

const contracts = new Map([
  ["app/elf", "idreeswatch_module"],
  ["theme/content", "theme.palette.v1"],
  ["watchface/content", "watchface.simple.v1"],
  ["wallpaper/content", "wallpaper.rgb565.v1"],
  ["icon-pack/content", "icon-pack.v1"],
  ["app/micropython", "micropython.v1"],
  ["firmware/ota", "firmware.esp32s3.signed.v1"],
]);
if (contracts.get(`${manifest.kind}/${manifest.runtime}`) !== manifest.entry) {
  throw new Error(`Unsupported package contract: ${manifest.kind}/${manifest.runtime}`);
}
if (manifest.runtime === "content") {
  if (payload.length > 768 * 1024) {
    throw new Error("Content payload cannot exceed 768 KiB");
  }
  const content = JSON.parse(payload.toString("utf8"));
  if (content.format !== manifest.entry) {
    throw new Error(`Payload format must be ${manifest.entry}`);
  }
}

const relativePackagePath =
  `packages/${manifest.id}/${manifest.version}/package.iwpkg`;
const relativeManifestPath =
  `packages/${manifest.id}/${manifest.version}/manifest.json`;
const publicPackagePath = resolve(root, "public", relativePackagePath);
const publicManifestPath = resolve(root, "public", relativeManifestPath);
await mkdir(dirname(publicPackagePath), { recursive: true });
await copyFile(payloadPath, publicPackagePath);
await copyFile(manifestPath, publicManifestPath);

const catalogPath = resolve(root, "public/v1/catalog.json");
const catalog = JSON.parse(await readFile(catalogPath, "utf8"));
const digest = createHash("sha256").update(payload).digest("hex");
const record = {
  id: manifest.id,
  name: manifest.name,
  version: manifest.version,
  author: manifest.author,
  summary: manifest.summary,
  kind: manifest.kind,
  runtime: manifest.runtime,
  entry: manifest.entry,
  ...(manifest.icon ? { icon: manifest.icon } : {}),
  min_os_api: manifest.min_os_api,
  size_bytes: payload.length,
  package_url:
    `https://idreeswatch.github.io/app-store/${relativePackagePath}`,
  sha256: digest,
  availability: "available",
};

const existingIndex = catalog.packages.findIndex(
  (candidate) => candidate.id === manifest.id,
);
if (existingIndex >= 0) catalog.packages[existingIndex] = record;
else catalog.packages.push(record);
catalog.generated_at = new Date().toISOString();
await writeFile(catalogPath, `${JSON.stringify(catalog, null, 2)}\n`);

process.stdout.write(
  `Published ${manifest.id}@${manifest.version} from ${basename(payloadPath)} ` +
    `(${payload.length} bytes, ${digest})\n`,
);
