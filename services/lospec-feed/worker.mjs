const FEED_KEY = "palettes-v1";
const PAGE_SIZE = 100;
const MAX_PAGES = 3;

function cleanPalette(value) {
  if (!value || typeof value.slug !== "string" || !/^[a-z0-9-]{1,128}$/.test(value.slug) ||
      typeof value.title !== "string" || !value.title.trim() ||
      !Number.isSafeInteger(value.likes) || value.likes < 0 ||
      !Array.isArray(value.colors) || (value.colors.length < 4 || value.colors.length > 16) ||
      !value.colors.every(color => typeof color === "string" && /^[0-9a-f]{6}$/i.test(color))) {
    return null;
  }
  return {
    slug: value.slug,
    title: value.title.slice(0, 80),
    colors: value.colors,
    likes: value.likes,
    user: { name: typeof value.user?.name === "string" ? value.user.name.slice(0, 80) : "" },
  };
}

export async function refreshFeed(env, fetchPage = fetch) {
  if (!env.LOSPEC_API_KEY) throw new Error("Missing LOSPEC_API_KEY secret");
  const apiKey = env.LOSPEC_API_KEY.trim();
  if (!env.PALETTES || typeof env.PALETTES.put !== "function") {
    throw new Error("Missing PALETTES storage binding");
  }
  const data = [];
  const seen = new Set();
  for (let page = 0; page < MAX_PAGES; page++) {
    const url = new URL("https://api.lospec.com/api/v1/palettes");
    url.search = new URLSearchParams({
      sort: "-publishedAt", minColors: "4", maxColors: "16",
      limit: String(PAGE_SIZE), offset: String(page * PAGE_SIZE), format: "expanded",
    }).toString();
    let response;
    try {
      response = await fetchPage(url, {
      headers: { Authorization: `Bearer ${apiKey}` },
      signal: AbortSignal.timeout(20000),
      // Workers supports manual/follow; manual keeps credentials on this host.
      redirect: "manual",
      });
    } catch (error) {
      let detail = error instanceof Error ? `${error.name}: ${error.message}` : "Unknown error";
      for (const secret of [env.LOSPEC_API_KEY, apiKey, env.FEED_ADMIN_TOKEN]) {
        if (secret) detail = detail.split(secret).join("[redacted]");
      }
      throw new Error(`Lospec network request failed: ${detail.slice(0, 240)}`);
    }
    if (!response.ok) throw new Error(`Lospec returned HTTP ${response.status}`);
    let body;
    try {
      body = await response.json();
    } catch {
      throw new Error("Lospec returned non-JSON response");
    }
    if (!Array.isArray(body.data) || body.data.length > PAGE_SIZE ||
        typeof body.meta?.hasMore !== "boolean") {
      throw new Error("Invalid Lospec response");
    }
    for (const value of body.data) {
      const palette = cleanPalette(value);
      if (palette && !seen.has(palette.slug)) {
        seen.add(palette.slug);
        data.push(palette);
      }
    }
    if (!body.meta.hasMore) break;
  }
  const feed = JSON.stringify({
    data,
    meta: { hasMore: false, fetchedAt: new Date().toISOString(), source: "https://lospec.com/palette-list/" },
  });
  // Replace the last good feed only after every upstream page succeeds.
  try {
    await env.PALETTES.put(FEED_KEY, feed);
  } catch {
    throw new Error("Palette storage write failed");
  }
  return feed;
}

function jsonResponse(body, status = 200, cache = "public, max-age=900") {
  return new Response(body, {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", "Cache-Control": cache },
  });
}

export default {
  async scheduled(_controller, env) {
    await refreshFeed(env);
  },

  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (path === "/refresh") {
      if (!env.FEED_ADMIN_TOKEN ||
          request.headers.get("Authorization") !== `Bearer ${env.FEED_ADMIN_TOKEN}`) {
        return jsonResponse('{"error":"Unauthorized"}', 401, "no-store");
      }
      if (request.method !== "POST") {
        return jsonResponse('{"error":"Use POST"}', 405, "no-store");
      }
      try {
        return jsonResponse(await refreshFeed(env), 200, "no-store");
      } catch (error) {
        const message = error instanceof Error ? error.message : "";
        const detail = message === "Missing LOSPEC_API_KEY secret" ||
            message === "Missing PALETTES storage binding" ||
            message.startsWith("Lospec network request failed:") ||
            message === "Lospec returned non-JSON response" ||
            message === "Palette storage write failed" ||
            message === "Invalid Lospec response" ||
            /^Lospec returned HTTP \d{3}$/.test(message)
          ? message : "Network or palette storage failed; check the PALETTES binding";
        return jsonResponse(JSON.stringify({ error: detail, previousFeedKept: true }), 502, "no-store");
      }
    }
    if (path !== "/palettes.json") return new Response("Not found", { status: 404 });
    if (request.method !== "GET" && request.method !== "HEAD") {
      return new Response("Method not allowed", { status: 405, headers: { Allow: "GET, HEAD" } });
    }
    // Public requests read the snapshot only. They never spend the Lospec quota.
    const feed = await env.PALETTES.get(FEED_KEY, { cacheTtl: 60 });
    if (feed === null) return jsonResponse('{"error":"Feed not ready"}', 503, "no-store");
    return jsonResponse(request.method === "HEAD" ? null : feed);
  },
};
