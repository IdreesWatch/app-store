import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { deflateSync } from "node:zlib";

const root = fileURLToPath(new URL("..", import.meta.url));
const id = "org.idreeswatch.wallpaper.neon-drift";
const directory = join(root, "wallpapers", id);
const width = 410;
const height = 502;

const clamp = (value) => Math.max(0, Math.min(255, Math.round(value)));
const rgb565 = Buffer.alloc(width * height * 2);
const rgba = Buffer.alloc(width * height * 4);

for (let y = 0; y < height; ++y) {
  for (let x = 0; x < width; ++x) {
    const nx = (x - width / 2) / width;
    const ny = (y - height / 2) / height;
    const vignette = Math.max(0, 1 - Math.hypot(nx * 1.2, ny) * 1.38);
    const waveA = Math.exp(-Math.pow((y - (128 + 42 * Math.sin(x / 73))) / 46, 2));
    const waveB = Math.exp(-Math.pow((y - (262 + 58 * Math.sin(x / 91 + 1.7))) / 64, 2));
    const waveC = Math.exp(-Math.pow((y - (392 + 32 * Math.sin(x / 52 + 3.1))) / 38, 2));
    const starHash = ((x * 92837111) ^ (y * 689287499)) >>> 0;
    const star = starHash % 997 === 0 ? 110 + (starHash % 110) : 0;
    const r = clamp(3 + vignette * 4 + waveA * 72 + waveB * 16 + waveC * 78 + star);
    const g = clamp(7 + vignette * 9 + waveA * 32 + waveB * 92 + waveC * 18 + star);
    const b = clamp(18 + vignette * 18 + waveA * 140 + waveB * 125 + waveC * 108 + star);
    const pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    const index = y * width + x;
    rgb565.writeUInt16LE(pixel, index * 2);
    rgba[index * 4] = r;
    rgba[index * 4 + 1] = g;
    rgba[index * 4 + 2] = b;
    rgba[index * 4 + 3] = 255;
  }
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

function chunk(type, data) {
  const name = Buffer.from(type);
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length);
  const checksum = Buffer.alloc(4);
  checksum.writeUInt32BE(crc32(Buffer.concat([name, data])));
  return Buffer.concat([length, name, data, checksum]);
}

function encodePng() {
  const scanlines = Buffer.alloc((width * 4 + 1) * height);
  for (let y = 0; y < height; ++y) {
    const offset = y * (width * 4 + 1);
    scanlines[offset] = 0;
    rgba.copy(scanlines, offset + 1, y * width * 4, (y + 1) * width * 4);
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header[8] = 8;
  header[9] = 6;
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    chunk("IHDR", header),
    chunk("IDAT", deflateSync(scanlines, { level: 9 })),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

const payload = {
  format: "wallpaper.rgb565.v1",
  width,
  height,
  pixel_format: "rgb565",
  data: rgb565.toString("base64"),
};
const manifest = {
  id,
  name: "Neon Drift",
  version: "1.0.0",
  author: "IdreesWatch",
  summary: "An AMOLED-black aurora field with electric ribbons and quiet stars.",
  kind: "wallpaper",
  runtime: "content",
  entry: "wallpaper.rgb565.v1",
  artifact: "wallpaper.json",
  icon: "watchfaces",
  license: "MIT",
  min_os_api: 1,
};

await mkdir(directory, { recursive: true });
await writeFile(join(directory, "wallpaper.json"), `${JSON.stringify(payload)}\n`);
await writeFile(join(directory, "manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
await writeFile(join(directory, "preview.png"), encodePng());
process.stdout.write("Built Neon Drift wallpaper package\n");
