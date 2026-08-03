import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";

const catalogUrl = new URL("../public/v1/catalog.json", import.meta.url);
const catalog = JSON.parse(await readFile(catalogUrl, "utf8"));
const allowedKinds = new Set(["app", "watchface", "theme", "wallpaper", "icon-pack", "firmware"]);
const allowedRuntimes = new Set(["native-host", "elf", "wasm", "content", "micropython", "ota"]);
const allowedAvailability = new Set(["available", "runtime-pending"]);
const packageIdPattern = /^(?!.*\.\.)[A-Za-z0-9][A-Za-z0-9._-]{2,62}$/;
const semverPattern = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
const sha256Pattern = /^[0-9a-f]{64}$/i;
const iconPattern = /^[a-z0-9][a-z0-9-]{0,30}$/;
const required = [
  "id",
  "name",
  "version",
  "author",
  "summary",
  "kind",
  "runtime",
  "entry",
  "min_os_api",
];

if (catalog.schema !== 1 || !Array.isArray(catalog.packages)) {
  throw new Error("Expected a schema-v1 package catalog");
}
if (catalog.packages.length > 32) {
  throw new Error("A catalog cannot exceed the watch's 32-package limit");
}

function requireString(packageInfo, key, maximumLength) {
  const value = packageInfo[key];
  if (typeof value !== "string" || value.length === 0 ||
      Buffer.byteLength(value, "utf8") > maximumLength) {
    throw new Error(`${packageInfo.id || "package"}.${key} must be 1-${maximumLength} UTF-8 bytes`);
  }
}

const ids = new Set();
for (const packageInfo of catalog.packages) {
  if (!packageInfo || typeof packageInfo !== "object" || Array.isArray(packageInfo)) {
    throw new Error("Every package must be an object");
  }
  for (const key of required) {
    if (!(key in packageInfo)) {
      throw new Error(`${packageInfo.id || "package"} is missing ${key}`);
    }
  }
  requireString(packageInfo, "id", 63);
  requireString(packageInfo, "name", 39);
  requireString(packageInfo, "version", 19);
  requireString(packageInfo, "author", 39);
  requireString(packageInfo, "summary", 111);
  requireString(packageInfo, "entry", 47);
  if (
    packageInfo.icon !== undefined &&
    (typeof packageInfo.icon !== "string" ||
     !iconPattern.test(packageInfo.icon))
  ) {
    throw new Error(`${packageInfo.id}.icon must be a stable icon id`);
  }
  if (!packageIdPattern.test(packageInfo.id)) {
    throw new Error(`Invalid package id: ${packageInfo.id}`);
  }
  if (!semverPattern.test(packageInfo.version)) {
    throw new Error(`${packageInfo.id} has an invalid semantic version`);
  }
  if (!allowedKinds.has(packageInfo.kind)) {
    throw new Error(`${packageInfo.id} has an unsupported kind`);
  }
  if (!allowedRuntimes.has(packageInfo.runtime)) {
    throw new Error(`${packageInfo.id} has an unsupported runtime`);
  }
  const supportedContract =
    (packageInfo.kind === "app" && packageInfo.runtime === "elf" &&
     packageInfo.entry === "idreeswatch_module") ||
    (packageInfo.kind === "app" && packageInfo.runtime === "native-host" &&
     packageInfo.entry.startsWith("host.")) ||
    (packageInfo.kind === "theme" && packageInfo.runtime === "content" &&
     packageInfo.entry === "theme.palette.v1") ||
    (packageInfo.kind === "watchface" && packageInfo.runtime === "content" &&
     packageInfo.entry === "watchface.simple.v1") ||
    (packageInfo.kind === "wallpaper" && packageInfo.runtime === "content" &&
     packageInfo.entry === "wallpaper.rgb565.v1") ||
    (packageInfo.kind === "icon-pack" && packageInfo.runtime === "content" &&
     packageInfo.entry === "icon-pack.v1") ||
    (packageInfo.kind === "app" && packageInfo.runtime === "micropython" &&
     packageInfo.entry === "micropython.v1") ||
    (packageInfo.kind === "firmware" && packageInfo.runtime === "ota" &&
     packageInfo.entry === "firmware.esp32s3.signed.v1");
  if (!supportedContract) {
    throw new Error(`${packageInfo.id} uses a contract unsupported by OS API 1`);
  }
  if (
    packageInfo.availability !== undefined &&
    !allowedAvailability.has(packageInfo.availability)
  ) {
    throw new Error(`${packageInfo.id} has an unsupported availability`);
  }
  if (!Number.isSafeInteger(packageInfo.min_os_api) ||
      packageInfo.min_os_api < 1 || packageInfo.min_os_api > 65535) {
    throw new Error(`${packageInfo.id}.min_os_api must be an integer from 1 through 65535`);
  }
  if (
    packageInfo.size_bytes !== undefined &&
    (!Number.isSafeInteger(packageInfo.size_bytes) || packageInfo.size_bytes < 0 ||
     packageInfo.size_bytes > 32 * 1024 * 1024)
  ) {
    throw new Error(`${packageInfo.id}.size_bytes must be a non-negative integer`);
  }
  if (packageInfo.package_url !== undefined) {
    if (Buffer.byteLength(packageInfo.package_url, "utf8") > 255) {
      throw new Error(`${packageInfo.id}.package_url exceeds the firmware limit`);
    }
    let packageUrl;
    try {
      packageUrl = new URL(packageInfo.package_url);
    } catch {
      throw new Error(`${packageInfo.id} has an invalid package_url`);
    }
    if (packageUrl.protocol !== "https:") {
      throw new Error(`${packageInfo.id}.package_url must use HTTPS`);
    }
    if (!sha256Pattern.test(packageInfo.sha256 || "")) {
      throw new Error(`${packageInfo.id} must provide a valid sha256 with package_url`);
    }
    if (
      packageUrl.hostname === "idreeswatch.github.io" &&
      packageUrl.pathname.startsWith("/app-store/")
    ) {
      const relativePath = packageUrl.pathname.slice("/app-store/".length);
      const payload = await readFile(
        new URL(`../public/${relativePath}`, import.meta.url),
      );
      const digest = createHash("sha256").update(payload).digest("hex");
      if (packageInfo.runtime === "content" && payload.length > 768 * 1024) {
        throw new Error(`${packageInfo.id} content payload exceeds 768 KiB`);
      }
      if (payload.length !== packageInfo.size_bytes) {
        throw new Error(`${packageInfo.id} size_bytes does not match its payload`);
      }
      if (digest.toLowerCase() !== packageInfo.sha256.toLowerCase()) {
        throw new Error(`${packageInfo.id} sha256 does not match its payload`);
      }
    }
  } else if (packageInfo.sha256 !== undefined) {
    throw new Error(`${packageInfo.id} provides sha256 without package_url`);
  }
  if (ids.has(packageInfo.id)) {
    throw new Error(`Duplicate package id: ${packageInfo.id}`);
  }
  ids.add(packageInfo.id);
}

process.stdout.write(`Validated ${catalog.packages.length} packages\n`);
