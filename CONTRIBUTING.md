# Publishing an app

Community apps are submitted by pull request. Create one directory:

```text
apps/<reverse-domain-package-id>/
├── CMakeLists.txt
├── manifest.json
├── README.md
├── main/
└── components/                 # optional
```

Use `apps/org.idreeswatch.nes` as the executable reference. Applications build
against `sdk/include/idreeswatch_module.h`; they do not include firmware
headers, LVGL, FreeRTOS, or board drivers. Declare the exact shared-object
filename in the manifest's `artifact` field; CI discovers and builds it
without app-specific workflow changes.

## Review and publishing

1. Fork the repository and add source plus a semantic-versioned manifest.
2. Open a pull request using the checklist.
3. CI builds the ESP32-S3 shared object from source and rejects unapproved
   firmware imports or bundled ROM/BIOS/game content.
4. Maintainer review covers lifecycle cleanup, memory bounds, responsiveness,
   licensing, and user-facing behavior.
5. After merge, CI builds the same source, hashes the result, adds it to the
   HTTPS catalog, and deploys the store.

Merge means “available to install.” The watch never auto-installs apps and
never auto-applies app updates.

## Sideloading

Open-source software does not require catalog approval. A developer may copy a
built module and its manifest to:

```text
/sdcard/IdreesWatch/system/import/manifest.json
/sdcard/IdreesWatch/system/import/package.iwpkg
```

The watch shows: **“Do you trust this package from an unsigned source?”**
before importing it. The package then uses the same install, launch, update,
and uninstall lifecycle as a curated app. Native ESP32-S3 modules are not a
security sandbox; only sideload code you trust.
