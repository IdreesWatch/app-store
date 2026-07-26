# NES reference application

This is the canonical downloadable IdreesWatchOS application. The emulator
core is compiled into an ESP32-S3 shared object; the firmware provides only
the portable module host. Install, update, launch, and uninstall therefore do
not reflash firmware.

Place legally obtained ROMs in:

```text
/sdcard/IdreesWatch/library/roms/nes/
```

Build with ESP-IDF 5.5:

```sh
idf.py set-target esp32s3
idf.py build
```

The `idreeswatch_nes.so` output is packaged and hashed by repository CI. No
ROM content is part of the build or package.
