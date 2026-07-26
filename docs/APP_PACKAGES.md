# IdreesWatchOS package platform

## What ships now

The package layer has three independent pieces:

1. A versioned HTTPS catalog (`schema: 1`).
2. An installation registry stored in NVS.
3. Runtime adapters that decide how a package executes.

The launcher never hard-codes store visibility. A core app has no
`package_id`; an optional app has one and appears only while that package is
installed.

Game Boy and DOOM currently use `native-host` runtimes. Their CPU-intensive
engines remain linked into the firmware for predictable performance, while
the store owns installation state, metadata, and content expectations.
tiny386 and NES are already reserved in the catalog but cannot be installed
until their real host runtimes are ported.

No copyrighted ROM, BIOS, or WAD content belongs in the repository or store.
Users provide legally obtained content on SD.

## Why native apps are not copied blindly from SD

An SD file is data, not a safe ESP-IDF component. Native Xtensa code needs
relocation, a stable symbol boundary, executable memory, lifecycle cleanup,
and signature verification. Treating a random binary as a function pointer
would make one bad package capable of crashing or taking over the OS.

The planned portable runtime is Espressif's `elf_loader`, which supports
running ESP32-S3 ELF code from PSRAM. It will sit behind the existing `elf`
runtime type. High-performance emulators may stay as reviewed native hosts;
ordinary third-party apps should use the restricted ELF SDK, and a future
WASM runtime can offer a stronger sandbox.

## Package kinds

- `app`: launchable UI or native-host feature.
- `watchface`: future watchface-manager provider.
- `theme`: future color/token and asset provider.
- `firmware`: future OTA image handled by the same catalog and verification
  pipeline, but written only through ESP-IDF OTA APIs.

The package kind and runtime are separate. For example, a watchface can later
be an ELF module or a declarative content package.

## Catalog format

```json
{
  "schema": 1,
  "generated_at": "2026-07-26T00:00:00Z",
  "packages": [
    {
      "id": "dev.example.timer",
      "name": "Interval Timer",
      "version": "1.0.0",
      "author": "Example Developer",
      "summary": "A compact interval timer.",
      "kind": "app",
      "runtime": "elf",
      "entry": "idreeswatch_module",
      "min_os_api": 1,
      "size_bytes": 24576,
      "package_url": "https://example.com/dev.example.timer/app.elf",
      "sha256": "64 lowercase or uppercase hexadecimal characters"
    }
  ]
}
```

Identifiers may contain ASCII letters, numbers, `.`, `_`, and `-`; `..` is
rejected. Every string and catalog count is bounded on-device. Duplicate IDs,
unknown enums, oversized values, and mismatched URL/hash fields reject the
whole remote catalog, leaving the compiled fallback intact.

The machine-readable contract is
[`sdk/app-package.schema.json`](../sdk/app-package.schema.json).

## Runtime contract

`native-host` package entries use a stable name such as `host.peanutgb`.
Firmware registers the entry only when the implementation is present. The
store disables Get when an entry is absent, so catalog metadata can never
claim a non-existent runtime works.

The ELF ABI exposes a single descriptor rather than the app manager. Its
public definition is
[`sdk/include/idreeswatch_module.h`](../sdk/include/idreeswatch_module.h):

```c
typedef struct {
    uint16_t abi_version;
    const char *id;
    esp_err_t (*create)(lv_obj_t *parent);
    void (*destroy)(void);
    void (*suspend)(void);
    void (*resume)(void);
} idreeswatch_module_v1_t;
```

Only reviewed wrapper functions should be exported to modules. Direct access
to NVS, Wi-Fi credentials, raw display transport, partition APIs, and arbitrary
filesystem paths must remain unavailable. Package permissions will be added
before public ELF loading is enabled.

## Repository and deployment

`app-store-server/public` is a static storefront and catalog suitable for
GitHub Pages, Render, a Raspberry Pi, or any ordinary web server. It exposes:

- `GET /health.json` (`/health` is also mapped on Render/local preview)
- `GET /v1/catalog.json`

The default firmware URL is:

```text
https://idreeswatch.github.io/app-store/v1/catalog.json
```

The watch still carries a compiled fallback catalog for offline use. GitHub
Pages is appropriate for catalog data and small signed modules; large binaries
can later move behind a CDN without changing the manifest contract.

## OTA migration

The current factory-only partition table cannot perform safe A/B OTA. Before
firmware packages are enabled:

1. Replace the factory layout with `ota_0`, `ota_1`, and `otadata`.
2. Keep application packages and user content on SD.
3. Require SHA-256 for every payload and signed firmware verification for OTA.
4. Download to the inactive OTA partition, validate, switch boot partition,
   and use rollback confirmation after a healthy boot.

App installation must never write arbitrary offsets in flash.
