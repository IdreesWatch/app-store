"use strict";

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const port = Number.parseInt(process.env.PORT || "3000", 10);
const publicRoot = path.join(__dirname, "public");
const catalogPath = path.join(publicRoot, "v1", "catalog.json");
const catalogText = fs.readFileSync(catalogPath, "utf8");
const catalog = JSON.parse(catalogText);

if (catalog.schema !== 1 || !Array.isArray(catalog.packages)) {
  throw new Error("catalog.json must be an IdreesWatch schema-v1 catalog");
}

const routes = {
  "/": ["index.html", "text/html; charset=utf-8"],
  "/index.html": ["index.html", "text/html; charset=utf-8"],
  "/styles.css": ["styles.css", "text/css; charset=utf-8"],
  "/app.js": ["app.js", "text/javascript; charset=utf-8"],
  "/health": ["health.json", "application/json; charset=utf-8"],
  "/health.json": ["health.json", "application/json; charset=utf-8"],
  "/v1/catalog.json": ["v1/catalog.json", "application/json; charset=utf-8"],
};

const server = http.createServer((request, response) => {
  if (request.method !== "GET") {
    response.writeHead(405, { allow: "GET" });
    response.end(JSON.stringify({ error: "method_not_allowed" }));
    return;
  }

  const route = routes[request.url];
  if (!route) {
    response.writeHead(404, { "content-type": "application/json" });
    response.end(JSON.stringify({ error: "not_found" }));
    return;
  }

  const [relativePath, contentType] = route;
  const body = fs.readFileSync(path.join(publicRoot, relativePath));
  response.writeHead(200, {
    "access-control-allow-origin": "*",
    "cache-control": request.url.startsWith("/health")
      ? "no-store"
      : "public, max-age=300",
    "content-type": contentType,
    "x-content-type-options": "nosniff",
  });
  response.end(body);
});

server.listen(port, "0.0.0.0", () => {
  process.stdout.write(`IdreesWatch store listening on ${port}\n`);
});
