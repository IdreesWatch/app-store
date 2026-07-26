import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { basename, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const [manifestArgument, payloadArgument] = process.argv.slice(2);
if (!manifestArgument || !payloadArgument) {
  throw new Error(
    "usage: node scripts/publish-built-app.mjs <manifest.json> <module.so>",
  );
}

const root = fileURLToPath(new URL("..", import.meta.url));
const manifestPath = resolve(manifestArgument);
const payloadPath = resolve(payloadArgument);
const manifest = JSON.parse(await readFile(manifestPath, "utf8"));
const payload = await readFile(payloadPath);

if (manifest.runtime !== "elf" || manifest.entry !== "idreeswatch_module") {
  throw new Error("Portable apps must use runtime=elf and entry=idreeswatch_module");
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
