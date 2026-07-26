# IdreesWatch App Store

One dependency-free static storefront and catalog for GitHub Pages, Render, or
any ordinary web server. The watch consumes `public/v1/catalog.json`; the site
renders the same file for people.

Community applications are submitted as source pull requests. CI builds the
ESP32-S3 module, checks its firmware imports and license metadata, hashes the
artifact, and publishes it only after review. See
[`CONTRIBUTING.md`](CONTRIBUTING.md) and the executable NES reference under
[`app-store-server/apps/org.idreeswatch.nes`](app-store-server/apps/org.idreeswatch.nes).

Catalog publication never installs or updates an app on a watch. Every install
and update remains an explicit user action; unsigned local modules can also be
sideloaded from SD after an on-watch trust warning.

```powershell
cd app-store-server
npm start
```

Then open `http://localhost:3000`.

## GitHub Pages

The repository workflow publishes `public/` automatically. In the GitHub
repository, select **Settings -> Pages -> Source -> GitHub Actions** once. The
catalog URL will be:

```text
https://idreeswatch.github.io/app-store/v1/catalog.json
```

## Render

Create a Blueprint with `app-store-server/render.yaml`, or create a Static Site
with:

- Root directory: `app-store-server`
- Build command: `node scripts/validate.mjs`
- Publish directory: `public`

Set the resulting HTTPS catalog URL through the firmware's
`IDREESWATCH_STORE_CATALOG_URL` option. Keep `catalog.json` in source control;
publishing a package becomes a reviewed source pull request. The generic CI
matrix discovers every app directory, builds and verifies its declared
artifact, and publishes the exact size and SHA-256 digest.

GitHub Pages is ideal for the catalog and small reviewed app modules. Large
artifacts should eventually move to object storage/CDN; copyrighted ROMs,
BIOS files, and WADs are never hosted.
