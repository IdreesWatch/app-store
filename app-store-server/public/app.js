"use strict";

const grid = document.querySelector("#app-grid");
const status = document.querySelector("#catalog-status");
const count = document.querySelector("#app-count");

const iconLabels = new Map([
  ["org.idreeswatch.peanutgb", "GB"],
  ["org.idreeswatch.doom", "D"],
  ["org.idreeswatch.tiny386", "386"],
  ["org.idreeswatch.nes", "NES"],
]);

function createCard(packageInfo) {
  const card = document.createElement("article");
  card.className = "app-card";

  const icon = document.createElement("div");
  icon.className = "app-icon";
  icon.setAttribute("aria-hidden", "true");
  icon.textContent = iconLabels.get(packageInfo.id) || "APP";

  const title = document.createElement("h3");
  title.className = "app-title";
  title.textContent = packageInfo.name;

  const summary = document.createElement("p");
  summary.className = "app-summary";
  summary.textContent = packageInfo.summary;

  const availability = document.createElement("span");
  const pending = packageInfo.availability === "runtime-pending";
  availability.className = `availability${pending ? " pending" : ""}`;
  availability.textContent = pending ? "Runtime pending" : "Available";

  const meta = document.createElement("div");
  meta.className = "app-meta";
  meta.textContent = `${packageInfo.runtime} · v${packageInfo.version}`;

  card.append(icon, title, availability, summary, meta);
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
    grid.replaceChildren(fragment);
    count.textContent = String(catalog.packages.length);
    status.textContent = `Schema v${catalog.schema} · ${catalog.packages.length} verified entries`;
  } catch (error) {
    status.textContent = "Catalog temporarily unavailable";
    grid.textContent = "";
  }
}

loadCatalog();
