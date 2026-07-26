import { readFile } from "node:fs/promises";
import { createHash } from "node:crypto";

const catalogUrl = new URL("../public/v1/catalog.json", import.meta.url);
const catalog = JSON.parse(await readFile(catalogUrl, "utf8"));
const allowedKinds = new Set(["app", "watchface", "theme", "firmware"]);
const allowedRuntimes = new Set(["native-host", "elf", "wasm", "content"]);
const allowedAvailability = new Set(["available", "runtime-pending"]);
const packageIdPattern = /^(?!.*\.\.)[A-Za-z0-9][A-Za-z0-9._-]{2,95}$/;
const semverPattern = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
const sha256Pattern = /^[0-9a-f]{64}$/i;
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

function requireString(packageInfo, key, maximumLength) {
  const value = packageInfo[key];
  if (typeof value !== "string" || value.length === 0 || value.length > maximumLength) {
    throw new Error(`${packageInfo.id || "package"}.${key} must be 1-${maximumLength} characters`);
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
  requireString(packageInfo, "id", 96);
  requireString(packageInfo, "name", 64);
  requireString(packageInfo, "version", 32);
  requireString(packageInfo, "author", 64);
  requireString(packageInfo, "summary", 160);
  requireString(packageInfo, "entry", 96);
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
  if (
    packageInfo.availability !== undefined &&
    !allowedAvailability.has(packageInfo.availability)
  ) {
    throw new Error(`${packageInfo.id} has an unsupported availability`);
  }
  if (!Number.isSafeInteger(packageInfo.min_os_api) || packageInfo.min_os_api < 1) {
    throw new Error(`${packageInfo.id}.min_os_api must be a positive integer`);
  }
  if (
    packageInfo.size_bytes !== undefined &&
    (!Number.isSafeInteger(packageInfo.size_bytes) || packageInfo.size_bytes < 0)
  ) {
    throw new Error(`${packageInfo.id}.size_bytes must be a non-negative integer`);
  }
  if (packageInfo.package_url !== undefined) {
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
