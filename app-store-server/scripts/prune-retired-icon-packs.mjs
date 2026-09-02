import { readFile, rm, writeFile } from "node:fs/promises";
import { join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL("..", import.meta.url));
const catalogPath = resolve(root, "public/v1/catalog.json");
const retired = new Set([
  "org.idreeswatch.icons.prism",
  "org.idreeswatch.icons.pixel-riot",
  "org.idreeswatch.icons.orbit-chrome",
  "org.idreeswatch.icons.blueprint-nodes",
]);

const catalog = JSON.parse(await readFile(catalogPath, "utf8"));
const before = catalog.packages.length;
catalog.packages = catalog.packages.filter((item) => !retired.has(item.id));
await writeFile(catalogPath, `${JSON.stringify(catalog, null, 2)}\n`);
for (const id of retired) {
  for (const path of [join(root, "icon-packs", id),
                      join(root, "public", "packages", id)]) {
    const relativePath = relative(root, resolve(path));
    if (relativePath.startsWith("..") || relativePath === "") {
      throw new Error(`Refusing to remove path outside app-store root: ${path}`);
    }
    await rm(path, { recursive: true, force: true });
  }
}
await rm(join(root, "public", "packages", "org.idreeswatch.icons.midnight",
  "1.0.0"), { recursive: true, force: true });
process.stdout.write(`Removed ${before - catalog.packages.length} retired catalog entries\n`);
