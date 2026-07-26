# IdreesWatch App Store

One dependency-free static storefront and catalog for GitHub Pages, Render, or
any ordinary web server. The watch consumes `public/v1/catalog.json`; the site
renders the same file for people.

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
publishing a package becomes a reviewed catalog change.

GitHub Pages is ideal for the catalog and small signed app modules. Large
artifacts should eventually move to object storage/CDN; copyrighted ROMs,
BIOS files, and WADs are never hosted.
