import { copyFile, mkdir, readFile, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import sharp from "sharp";

const storeRoot = fileURLToPath(new URL("..", import.meta.url));
const repositoryRoot = resolve(storeRoot, "..");
const modulesRoot = join(repositoryRoot, "node_modules");
const side = 44;

const iconIds = [
  "store", "settings", "calculator", "weather", "translate", "assistant",
  "music", "clock", "watchfaces", "files", "news", "gamepad", "doom",
];

const colors = {
  store: "#3DDCFF",
  settings: "#B58CFF",
  calculator: "#4DE6B8",
  weather: "#FFD84A",
  translate: "#5EA4FF",
  assistant: "#FF79DC",
  music: "#FF668F",
  clock: "#52C7FF",
  watchfaces: "#6DE6A2",
  files: "#FFB74D",
  news: "#FF7D70",
  gamepad: "#8FE56E",
  doom: "#FF5252",
};

const packs = [
  {
    id: "org.idreeswatch.icons.material-rounded",
    name: "Material Rounded",
    version: "1.0.0",
    author: "Google",
    summary: "Google Material Symbols Rounded icon pack.",
    license: "Apache-2.0",
    licensePath: join(modulesRoot, "@material-symbols/svg-400/LICENSE"),
    source: "https://github.com/google/material-design-icons",
    family: "material",
    names: {
      store: "storefront", settings: "settings", calculator: "calculate",
      weather: "partly_cloudy_day", translate: "translate",
      assistant: "wand_stars", music: "music_note", clock: "schedule",
      watchfaces: "watch", files: "folder", news: "newspaper",
      gamepad: "stadia_controller", doom: "skull",
    },
  },
  {
    id: "org.idreeswatch.icons.phosphor-duotone",
    name: "Phosphor Duotone",
    version: "1.0.0",
    author: "Phosphor Icons",
    summary: "Phosphor Duotone icon pack.",
    license: "MIT",
    licensePath: join(modulesRoot, "@phosphor-icons/core/LICENSE"),
    source: "https://github.com/phosphor-icons/core",
    family: "phosphor",
    names: {
      store: "storefront", settings: "gear", calculator: "calculator",
      weather: "cloud-sun", translate: "translate", assistant: "sparkle",
      music: "music-notes", clock: "clock", watchfaces: "watch",
      files: "folder", news: "newspaper", gamepad: "game-controller",
      doom: "skull",
    },
  },
  {
    id: "org.idreeswatch.icons.lucide-neon",
    name: "Lucide",
    version: "1.0.0",
    author: "Lucide Contributors",
    summary: "Lucide outline icons for the app launcher.",
    license: "ISC",
    licensePath: join(modulesRoot, "lucide-static/LICENSE"),
    source: "https://github.com/lucide-icons/lucide",
    family: "lucide",
    names: {
      store: "store", settings: "settings", calculator: "calculator",
      weather: "cloud-sun", translate: "languages", assistant: "sparkles",
      music: "music", clock: "clock-3", watchfaces: "watch",
      files: "folder", news: "newspaper", gamepad: "gamepad-2", doom: "skull",
    },
  },
];

function toBgra(rgba) {
  const output = Buffer.alloc(rgba.length);
  for (let offset = 0; offset < rgba.length; offset += 4) {
    output[offset] = rgba[offset + 2];
    output[offset + 1] = rgba[offset + 1];
    output[offset + 2] = rgba[offset];
    output[offset + 3] = rgba[offset + 3];
  }
  return output;
}

async function sourceSvg(pack, id) {
  const name = pack.names[id];
  let path;
  if (pack.family === "material") {
    path = join(modulesRoot, "@material-symbols/svg-400/rounded",
      `${name}-fill.svg`);
  } else if (pack.family === "phosphor") {
    path = join(modulesRoot, "@phosphor-icons/core/assets/duotone",
      `${name}-duotone.svg`);
  } else {
    path = join(modulesRoot, "lucide-static/icons", `${name}.svg`);
  }
  let svg = await readFile(path, "utf8");
  if (pack.family === "material") {
    svg = svg.replace("<svg ", `<svg fill="${colors[id]}" `);
  } else {
    svg = svg.replaceAll("currentColor", colors[id]);
  }
  return Buffer.from(svg);
}

async function renderIcon(pack, id) {
  const svg = await sourceSvg(pack, id);
  const size = pack.family === "material" ? 36 : 35;
  const glyph = await sharp(svg).resize(size, size, {
    fit: "contain",
  }).png().toBuffer();
  const offset = Math.floor((side - size) / 2);

  if (pack.family !== "lucide") {
    return sharp({
      create: {
        width: side,
        height: side,
        channels: 4,
        background: { r: 0, g: 0, b: 0, alpha: 0 },
      },
    }).composite([{ input: glyph, left: offset, top: offset }])
      .raw().toBuffer();
  }

  const halo = await sharp(glyph).blur(2.2).modulate({ brightness: 1.15 })
    .png().toBuffer();
  return sharp({
    create: {
      width: side,
      height: side,
      channels: 4,
      background: { r: 0, g: 0, b: 0, alpha: 0 },
    },
  }).composite([
    { input: halo, left: offset, top: offset, blend: "screen" },
    { input: glyph, left: offset, top: offset },
  ]).raw().toBuffer();
}

async function preview(images) {
  const scale = 3;
  const columns = 5;
  const gap = 14;
  const tile = side * scale;
  const rows = Math.ceil(images.length / columns);
  const composites = [];
  for (let index = 0; index < images.length; ++index) {
    composites.push({
      input: await sharp(images[index], {
        raw: { width: side, height: side, channels: 4 },
      }).resize(tile, tile, { kernel: "nearest" }).png().toBuffer(),
      left: gap + (index % columns) * (tile + gap),
      top: gap + Math.floor(index / columns) * (tile + gap),
    });
  }
  return sharp({
    create: {
      width: gap + columns * (tile + gap),
      height: gap + rows * (tile + gap),
      channels: 4,
      background: "#060A14",
    },
  }).composite(composites).png().toBuffer();
}

for (const pack of packs) {
  const directory = join(storeRoot, "icon-packs", pack.id);
  await mkdir(directory, { recursive: true });
  const entries = {};
  const images = [];
  for (const id of iconIds) {
    const image = await renderIcon(pack, id);
    images.push(image);
    entries[id] = {
      color: colors[id],
      data: toBgra(image).toString("base64"),
    };
  }
  const payload = {
    format: "icon-pack.v1",
    pixel_format: "argb8888",
    width: side,
    height: side,
    icons: entries,
  };
  const manifest = {
    id: pack.id,
    name: pack.name,
    version: pack.version,
    author: pack.author,
    summary: pack.summary,
    kind: "icon-pack",
    runtime: "content",
    entry: "icon-pack.v1",
    artifact: "icons.json",
    icon: "settings",
    license: pack.license,
    min_os_api: 1,
  };
  await writeFile(join(directory, "icons.json"), `${JSON.stringify(payload)}\n`);
  await writeFile(join(directory, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`);
  await writeFile(join(directory, "SOURCE"), `${pack.source}\n`);
  await copyFile(pack.licensePath, join(directory, "LICENSE"));
  await writeFile(join(directory, "preview.png"), await preview(images));
}

process.stdout.write("Built Material Rounded, Phosphor Duotone, and Lucide\n");
