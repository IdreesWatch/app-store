"use strict";

const list = document.querySelector("#app-grid");
const status = document.querySelector("#catalog-status");

function formatSize(bytes) {
  if (!Number.isFinite(bytes) || bytes <= 0) return null;
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${Math.ceil(bytes / 1024)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

function createCard(packageInfo) {
  const card = document.createElement("article");
  card.className = "app-card";

  const heading = document.createElement("div");
  heading.className = "app-heading";

  const title = document.createElement("h3");
  title.className = "app-title";
  title.textContent = packageInfo.name;

  const version = document.createElement("span");
  version.className = "app-version";
  version.textContent = `v${packageInfo.version}`;
  heading.append(title, version);

  const pending = packageInfo.availability === "runtime-pending";
  const availability = document.createElement("span");
  availability.className = `availability${pending ? " pending" : ""}`;
  availability.textContent = pending ? "Coming later" : "Available";

  const summary = document.createElement("p");
  summary.className = "app-summary";
  summary.textContent = packageInfo.summary;

  const meta = document.createElement("div");
  meta.className = "app-meta";
  const details = [
    packageInfo.author,
    packageInfo.runtime,
    formatSize(packageInfo.size_bytes),
  ].filter(Boolean);
  for (const detail of details) {
    const item = document.createElement("span");
    item.textContent = detail;
    meta.append(item);
  }

  card.append(heading, availability, summary, meta);

  if (!pending && packageInfo.package_url) {
    const download = document.createElement("a");
    download.className = "package-action";
    download.href = packageInfo.package_url;
    download.textContent = "Download";
    download.setAttribute("download", "");
    download.setAttribute(
      "aria-label",
      `Download ${packageInfo.name} package`,
    );
    card.append(download);
  }

  return card;
}

async function loadCatalog() {
  try {
    const response = await fetch("./v1/catalog.json", { cache: "no-cache" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);

    const catalog = await response.json();
    if (catalog.schema !== 1 || !Array.isArray(catalog.packages)) {
      throw new Error("Unsupported catalog");
    }

    const fragment = document.createDocumentFragment();
    for (const packageInfo of catalog.packages) {
      fragment.append(createCard(packageInfo));
    }
    list.replaceChildren(fragment);
    status.textContent = `${catalog.packages.length} apps`;
  } catch (error) {
    status.textContent = "Catalog unavailable";
    list.textContent = "";
  }
}

loadCatalog();
