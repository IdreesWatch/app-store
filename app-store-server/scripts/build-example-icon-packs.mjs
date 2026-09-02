import { mkdir, readFile, writeFile } from "node:fs/promises";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { deflateSync } from "node:zlib";

const storeRoot = fileURLToPath(new URL("..", import.meta.url));
const repositoryRoot = resolve(storeRoot, "..");
const assetsRoot = join(repositoryRoot, "components/assets/src");
const side = 44;

const icons = [
  ["store", "icon_app_store_44"],
  ["settings", "icon_settings_44"],
  ["calculator", "app_calculator_112_112"],
  ["weather", "app_weather_112_112"],
  ["translate", "app_translate_112_112"],
  ["assistant", "icon_gemini_112_112"],
  ["music", "app_music_112_112"],
  ["clock", "app_clock_112_112"],
  ["watchfaces", "icon_watchfaces_112_112"],
  ["files", "icon_file_manager_112_112"],
  ["news", "icon_news_112_112"],
  ["gamepad", "icon_game_boy_112_112"],
  ["doom", "icon_doom_112_112"],
];

const palettes = {
  store: ["#70f6ff", "#665cff", "#031833"],
  settings: ["#f7a8ff", "#8647ff", "#180a32"],
  calculator: ["#7effcf", "#00a8b8", "#04272a"],
  weather: ["#fff36e", "#ff7a3d", "#402006"],
  translate: ["#77f4ff", "#2575ff", "#061943"],
  assistant: ["#ff8ee7", "#735bff", "#26083b"],
  music: ["#ff96bc", "#ff315f", "#3b0719"],
  clock: ["#9efff0", "#0b9eff", "#07253d"],
  watchfaces: ["#a9ff9b", "#00b889", "#063326"],
  files: ["#ffe270", "#ff9838", "#3b2306"],
  news: ["#ff9f86", "#ed335f", "#3a0918"],
  gamepad: ["#baff80", "#21c267", "#07321c"],
  doom: ["#ff9d5c", "#df183f", "#3d0509"],
};

const clamp = (value, low = 0, high = 255) =>
  Math.max(low, Math.min(high, Math.round(value)));

function parseHex(hex) {
  const value = Number.parseInt(hex.slice(1), 16);
  return [(value >> 16) & 255, (value >> 8) & 255, value & 255];
}

function mix(a, b, amount) {
  return a.map((value, index) => value + (b[index] - value) * amount);
}

function rgbaBuffer() {
  return Buffer.alloc(side * side * 4);
}

function composite(buffer, x, y, rgb, alpha) {
  if (x < 0 || x >= side || y < 0 || y >= side || alpha <= 0) return;
  const offset = (y * side + x) * 4;
  const sourceAlpha = clamp(alpha) / 255;
  const destinationAlpha = buffer[offset + 3] / 255;
  const outputAlpha = sourceAlpha + destinationAlpha * (1 - sourceAlpha);
  if (outputAlpha <= 0) return;
  for (let channel = 0; channel < 3; ++channel) {
    buffer[offset + channel] = clamp(
      (rgb[channel] * sourceAlpha +
        buffer[offset + channel] * destinationAlpha * (1 - sourceAlpha)) /
        outputAlpha,
    );
  }
  buffer[offset + 3] = clamp(outputAlpha * 255);
}

function roundedRectCoverage(x, y, left, top, right, bottom, radius) {
  const centerX = Math.max(left + radius, Math.min(right - radius, x));
  const centerY = Math.max(top + radius, Math.min(bottom - radius, y));
  const distance = Math.hypot(x - centerX, y - centerY);
  return clamp((radius + 0.75 - distance) * 255);
}

function circleCoverage(x, y, centerX, centerY, radius) {
  return clamp((radius + 0.75 - Math.hypot(x - centerX, y - centerY)) * 255);
}

function blurMask(mask, radius) {
  const output = Buffer.alloc(mask.length);
  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      let sum = 0;
      let weight = 0;
      for (let yy = -radius; yy <= radius; ++yy) {
        for (let xx = -radius; xx <= radius; ++xx) {
          const sourceX = x + xx;
          const sourceY = y + yy;
          if (sourceX < 0 || sourceX >= side || sourceY < 0 || sourceY >= side) continue;
          const sampleWeight = radius + 1 - Math.max(Math.abs(xx), Math.abs(yy));
          sum += mask[sourceY * side + sourceX] * sampleWeight;
          weight += sampleWeight;
        }
      }
      output[y * side + x] = clamp(sum / weight);
    }
  }
  return output;
}

function renderAurora(mask, colors) {
  const output = rgbaBuffer();
  const bright = parseHex(colors[0]);
  const vivid = parseHex(colors[1]);
  const deep = parseHex(colors[2]);
  const glow = blurMask(mask, 3);

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const tile = roundedRectCoverage(x + 0.5, y + 0.5, 1.5, 1.5, 42.5, 42.5, 12);
      if (!tile) continue;
      const diagonal = clamp((x + y) / 86, 0, 1);
      const base = mix(deep, mix(vivid, deep, 0.64), diagonal);
      composite(output, x, y, base, tile * 0.92);

      const rimOuter = tile;
      const rimInner = roundedRectCoverage(x + 0.5, y + 0.5, 3, 3, 41, 41, 10.5);
      composite(output, x, y, mix(bright, [255, 255, 255], 0.44),
        Math.max(0, rimOuter - rimInner) * 0.85);

      const highlight = roundedRectCoverage(x + 0.5, y + 0.5, 5, 4, 39, 18, 8);
      composite(output, x, y, [255, 255, 255], highlight * (1 - y / 22) * 0.16);

      const halo = Math.max(0, glow[y * side + x] - mask[y * side + x] * 0.45);
      composite(output, x, y, vivid, halo * 0.52);
    }
  }

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const alpha = mask[y * side + x];
      if (!alpha) continue;
      const glyph = mix([255, 255, 255], bright, 0.28 + (y / side) * 0.62);
      composite(output, x, y, glyph, alpha);
    }
  }
  composite(output, 9, 8, [255, 255, 255], 245);
  composite(output, 10, 8, [255, 255, 255], 150);
  return output;
}

function renderSolar(mask, colors, iconIndex) {
  const output = rgbaBuffer();
  const bright = parseHex(colors[0]);
  const vivid = parseHex(colors[1]);
  const ink = parseHex(colors[2]);
  const shapeKind = iconIndex % 4;

  function insideShape(px, py) {
    if (shapeKind === 0) {
      return Math.abs(px - 22) / 20 + Math.abs(py - 22) / 18 <= 1;
    }
    if (shapeKind === 1) {
      const skewedX = px - (py - 22) * 0.18;
      return roundedRectCoverage(skewedX, py, 2, 4, 42, 40, 8) > 127;
    }
    if (shapeKind === 2) {
      return px >= 3 && px <= 41 && py >= 4 && py <= 40 &&
        Math.abs(px - 22) + Math.max(0, Math.abs(py - 22) - 11) <= 29;
    }
    return roundedRectCoverage(px, py, 1.5, 7, 42.5, 37, 15) > 127;
  }

  function shapeCoverage(x, y, offsetX = 0, offsetY = 0) {
    let hits = 0;
    for (const sy of [0.2, 0.8]) {
      for (const sx of [0.2, 0.8]) {
        if (insideShape(x + sx - offsetX, y + sy - offsetY)) ++hits;
      }
    }
    return hits * 63.75;
  }

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const shadow = shapeCoverage(x, y, 2.5, 3);
      composite(output, x, y, ink, shadow * 0.86);
      const shape = shapeCoverage(x, y);
      if (!shape) continue;
      const split = ((x + y + iconIndex * 7) % 23) < 7;
      composite(output, x, y, split ? bright : vivid, shape);
      if ((x * 2 + y + iconIndex * 5) % 17 < 2 && x < 13) {
        composite(output, x, y, [255, 255, 255], shape * 0.48);
      }
    }
  }

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const alpha = mask[y * side + x];
      if (!alpha) continue;
      composite(output, x + 1, y + 1, [255, 255, 255], alpha * 0.48);
      composite(output, x, y, ink, alpha);
    }
  }
  composite(output, 7 + (iconIndex % 3) * 2, 7, [255, 255, 255], 255);
  composite(output, 8 + (iconIndex % 3) * 2, 7, [255, 255, 255], 255);
  return output;
}

function renderPixel(mask, colors, iconIndex) {
  const output = rgbaBuffer();
  const bright = parseHex(colors[0]);
  const vivid = parseHex(colors[1]);
  const ink = parseHex(colors[2]);
  const block = 2;

  function frameInside(x, y) {
    return x >= 2 && x < 42 && y >= 2 && y < 42 &&
      !((x < 7 || x >= 37) && (y < 7 || y >= 37));
  }

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      if (frameInside(x - 2, y - 2)) composite(output, x, y, [0, 0, 0], 215);
      if (!frameInside(x, y)) continue;
      const edge = !frameInside(x - 1, y) || !frameInside(x + 1, y) ||
        !frameInside(x, y - 1) || !frameInside(x, y + 1);
      if (edge) {
        composite(output, x, y, (x + y) % 4 < 2 ? bright : vivid, 255);
      } else {
        const checker = ((Math.floor(x / 4) + Math.floor(y / 4) + iconIndex) & 1) === 0;
        composite(output, x, y, checker ? ink : mix(ink, vivid, 0.18), 250);
      }
    }
  }

  for (let by = 0; by < side; by += block) {
    for (let bx = 0; bx < side; bx += block) {
      let sum = 0;
      for (let y = by; y < Math.min(side, by + block); ++y) {
        for (let x = bx; x < Math.min(side, bx + block); ++x) {
          sum += mask[y * side + x];
        }
      }
      if (sum < 255) continue;
      const glyph = ((bx / block + by / block + iconIndex) % 5 === 0)
        ? bright : [245, 250, 255];
      for (let y = by; y < Math.min(side, by + block); ++y) {
        for (let x = bx; x < Math.min(side, bx + block); ++x) {
          composite(output, x + 1, y + 1, vivid, 220);
          composite(output, x, y, glyph, 255);
        }
      }
    }
  }
  return output;
}

function renderOrbit(mask, colors, iconIndex) {
  const output = rgbaBuffer();
  const bright = parseHex(colors[0]);
  const vivid = parseHex(colors[1]);
  const ink = parseHex(colors[2]);
  const glow = blurMask(mask, 2);
  const angle = -0.48 + (iconIndex % 3) * 0.16;
  const cos = Math.cos(angle);
  const sin = Math.sin(angle);

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const dx = x + 0.5 - 22;
      const dy = y + 0.5 - 22;
      const rx = dx * cos - dy * sin;
      const ry = dx * sin + dy * cos;
      const ellipse = Math.sqrt((rx * rx) / (20 * 20) + (ry * ry) / (11.5 * 11.5));
      const ring = clamp((0.075 - Math.abs(ellipse - 1)) * 3400);
      composite(output, x, y, mix(vivid, bright, (x + y) / 88), ring * 0.9);
      const halo = Math.max(0, glow[y * side + x] - mask[y * side + x] * 0.55);
      composite(output, x, y, vivid, halo * 0.42);
      if (x > 0 && y > 1) {
        composite(output, x, y, [0, 0, 0], mask[(y - 2) * side + x - 1] * 0.76);
      }
    }
  }

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const alpha = mask[y * side + x];
      if (!alpha) continue;
      const band = (y % 12) / 12;
      let chrome;
      if (band < 0.2) chrome = [255, 255, 255];
      else if (band < 0.48) chrome = mix(bright, [210, 220, 235], 0.62);
      else if (band < 0.7) chrome = mix(ink, [70, 80, 105], 0.42);
      else chrome = mix(vivid, [245, 250, 255], 0.74);
      composite(output, x, y, chrome, alpha);
    }
  }

  const planetAngle = 0.8 + iconIndex * 0.57;
  const planetX = 22 + Math.cos(planetAngle) * 18;
  const planetY = 22 + Math.sin(planetAngle) * 10;
  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const planet = circleCoverage(x + 0.5, y + 0.5, planetX, planetY, 2.4);
      composite(output, x, y, bright, planet);
    }
  }
  return output;
}

function renderBlueprint(mask, colors, iconIndex) {
  const output = rgbaBuffer();
  const bright = parseHex(colors[0]);
  const vivid = parseHex(colors[1]);
  const ink = mix(parseHex(colors[2]), [2, 13, 34], 0.6);

  for (let y = 0; y < side; ++y) {
    for (let x = 0; x < side; ++x) {
      const card = roundedRectCoverage(x + 0.5, y + 0.5, 2, 2, 42, 42, 5);
      if (!card) continue;
      composite(output, x, y, ink, card * 0.96);
      if ((x - 4) % 6 === 0 || (y - 4) % 6 === 0) {
        composite(output, x, y, vivid, card * 0.16);
      }
      const border = card - roundedRectCoverage(x + 0.5, y + 0.5, 3.5, 3.5, 40.5, 40.5, 3.7);
      composite(output, x, y, bright, Math.max(0, border) * 0.88);
    }
  }

  for (let y = 1; y < side - 1; ++y) {
    for (let x = 1; x < side - 1; ++x) {
      const alpha = mask[y * side + x];
      if (alpha < 42) continue;
      let neighborMin = 255;
      for (let yy = -1; yy <= 1; ++yy) {
        for (let xx = -1; xx <= 1; ++xx) {
          neighborMin = Math.min(neighborMin, mask[(y + yy) * side + x + xx]);
        }
      }
      const edge = clamp(alpha - neighborMin + (alpha < 220 ? 75 : 0));
      if (edge) composite(output, x, y, (x + y + iconIndex) % 9 < 3 ? bright : [220, 250, 255], edge);
    }
  }

  const nodes = [[7, 7], [36, 8 + iconIndex % 5], [8 + iconIndex % 4, 36]];
  for (const [nodeX, nodeY] of nodes) {
    for (let y = nodeY - 2; y <= nodeY + 2; ++y) {
      for (let x = nodeX - 2; x <= nodeX + 2; ++x) {
        composite(output, x, y, bright,
          circleCoverage(x + 0.5, y + 0.5, nodeX, nodeY, 1.6));
      }
    }
  }
  return output;
}

function toBgra(rgba) {
  const bgra = Buffer.alloc(rgba.length);
  for (let offset = 0; offset < rgba.length; offset += 4) {
    bgra[offset] = rgba[offset + 2];
    bgra[offset + 1] = rgba[offset + 1];
    bgra[offset + 2] = rgba[offset];
    bgra[offset + 3] = rgba[offset + 3];
  }
  return bgra;
}

async function readMask(stem) {
  const source = await readFile(join(assetsRoot, `${stem}_filled.c`), "utf8");
  const match = source.match(/_map\[\]\s*=\s*\{([\s\S]*?)\};/);
  if (!match) throw new Error(`No pixel map in ${stem}_filled.c`);
  const bytes = [...match[1].matchAll(/0x([0-9a-fA-F]{2})/g)]
    .map((entry) => Number.parseInt(entry[1], 16));
  if (bytes.length !== side * side) {
    throw new Error(`${stem}_filled has ${bytes.length} bytes; expected ${side * side}`);
  }
  return Buffer.from(bytes);
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; ++bit) {
      crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type, data) {
  const name = Buffer.from(type);
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  const checksum = Buffer.alloc(4);
  checksum.writeUInt32BE(crc32(Buffer.concat([name, data])));
  return Buffer.concat([length, name, data, checksum]);
}

function encodePng(width, height, rgba) {
  const rows = Buffer.alloc((width * 4 + 1) * height);
  for (let y = 0; y < height; ++y) {
    const destination = y * (width * 4 + 1);
    rows[destination] = 0;
    rgba.copy(rows, destination + 1, y * width * 4, (y + 1) * width * 4);
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header[8] = 8;
  header[9] = 6;
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    pngChunk("IHDR", header),
    pngChunk("IDAT", deflateSync(rows, { level: 9 })),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function createPreview(images) {
  const scale = 3;
  const columns = 5;
  const rows = Math.ceil(images.length / columns);
  const gap = 14;
  const tile = side * scale;
  const width = gap + columns * (tile + gap);
  const height = gap + rows * (tile + gap);
  const preview = Buffer.alloc(width * height * 4);
  for (let offset = 0; offset < preview.length; offset += 4) {
    preview[offset] = 6;
    preview[offset + 1] = 10;
    preview[offset + 2] = 20;
    preview[offset + 3] = 255;
  }
  images.forEach((image, index) => {
    const originX = gap + (index % columns) * (tile + gap);
    const originY = gap + Math.floor(index / columns) * (tile + gap);
    for (let y = 0; y < side; ++y) {
      for (let x = 0; x < side; ++x) {
        const source = (y * side + x) * 4;
        for (let yy = 0; yy < scale; ++yy) {
          for (let xx = 0; xx < scale; ++xx) {
            const destination = ((originY + y * scale + yy) * width +
              originX + x * scale + xx) * 4;
            const alpha = image[source + 3] / 255;
            for (let channel = 0; channel < 3; ++channel) {
              preview[destination + channel] = clamp(
                image[source + channel] * alpha +
                preview[destination + channel] * (1 - alpha),
              );
            }
          }
        }
      }
    }
  });
  return encodePng(width, height, preview);
}

async function buildPack({ id, name, version = "2.0.0", summary, renderer }) {
  const directory = join(storeRoot, "icon-packs", id);
  await mkdir(directory, { recursive: true });
  const entries = {};
  const previewImages = [];
  for (let index = 0; index < icons.length; ++index) {
    const [iconId, stem] = icons[index];
    const mask = await readMask(stem);
    const rendered = renderer(mask, palettes[iconId], index);
    previewImages.push(rendered);
    entries[iconId] = {
      color: palettes[iconId][0],
      data: toBgra(rendered).toString("base64"),
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
    id,
    name,
    version,
    author: "IdreesWatch",
    summary,
    kind: "icon-pack",
    runtime: "content",
    entry: "icon-pack.v1",
    artifact: "icons.json",
    icon: "settings",
    license: "MIT",
    min_os_api: 1,
  };
  await writeFile(join(directory, "icons.json"), `${JSON.stringify(payload)}\n`);
  await writeFile(join(directory, "manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
  await writeFile(join(directory, "preview.png"), createPreview(previewImages));
}

await buildPack({
  id: "org.idreeswatch.icons.midnight",
  name: "Aurora Glass",
  summary: "Colour icon pack with glass backgrounds.",
  renderer: renderAurora,
});
process.stdout.write("Built Aurora Glass icon pack\n");
