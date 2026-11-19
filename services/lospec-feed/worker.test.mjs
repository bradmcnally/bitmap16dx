import test from "node:test";
import assert from "node:assert/strict";
import worker, { refreshFeed } from "./worker.mjs";

const palette = index => ({
  slug: `test-${index}`, title: `Palette ${index}`, likes: 12,
  colors: ["574368", "8488d3", "cfd3c1", "f8c868"], user: { name: "Artist" },
});

function environment() {
  const entries = new Map();
  return {
    LOSPEC_API_KEY: "test-upstream-key", FEED_ADMIN_TOKEN: "test-admin-token",
    PALETTES: {
      async get(key) { return entries.get(key) ?? null; },
      async put(key, value) { entries.set(key, value); },
    },
  };
}

test("scheduled refresh caps upstream calls and strips unrelated fields", async () => {
  const env = environment();
  let calls = 0;
  const feed = JSON.parse(await refreshFeed(env, async (url, options) => {
    assert.equal(new URL(url).searchParams.get("offset"), String(calls * 100));
    assert.equal(new URL(url).searchParams.get("sort"), "-publishedAt");
    assert.equal(options.headers.Authorization, `Bearer ${env.LOSPEC_API_KEY}`);
    assert.equal(options.redirect, "manual");
    calls++;
    return Response.json({ data: [{ ...palette(calls), unrelated: "omit me" }], meta: { hasMore: true } });
  }));
  assert.equal(calls, 3);
  assert.equal(feed.data.length, 3);
  assert.equal(feed.meta.hasMore, false);
  assert.equal(feed.data[0].unrelated, undefined);
  assert.ok(!JSON.stringify(feed).includes(env.LOSPEC_API_KEY));
  assert.equal(JSON.parse(await env.PALETTES.get("palettes-v1")).data.length, 3);
});

test("preserves the previous feed when any page fails", async () => {
  const env = environment();
  await env.PALETTES.put("palettes-v1", "previous feed");
  let calls = 0;
  await assert.rejects(refreshFeed(env, async () => {
    if (calls++) return new Response("Rate limited", { status: 429 });
    return Response.json({ data: [palette(0)], meta: { hasMore: true } });
  }));
  assert.equal(await env.PALETTES.get("palettes-v1"), "previous feed");
});

test("filters malformed and unsupported palettes and removes duplicates", async () => {
  const env = environment();
  const feed = JSON.parse(await refreshFeed(env, async () => Response.json({
    data: [palette(0), palette(0), { ...palette(1), colors: Array(32).fill("abcdef") },
      { ...palette(2), colors: ["nothex", "ffffff", "ffffff", "ffffff"] },
      { ...palette(3), slug: "../bad" }, { ...palette(4), likes: "12" },
      { ...palette(5), slug: null }, { ...palette(6), slug: undefined }],
    meta: { hasMore: false },
  })));
  assert.deepEqual(feed.data.map(item => item.slug), ["test-0"]);
});

test("keeps every palette size from four through sixteen", async () => {
  const env = environment();
  const sizes = [3, ...Array.from({ length: 13 }, (_, i) => i + 4), 17];
  const feed = JSON.parse(await refreshFeed(env, async () => Response.json({
    data: sizes.map(size => ({ ...palette(size), colors: Array(size).fill("abcdef") })),
    meta: { hasMore: false },
  })));
  assert.deepEqual(feed.data.map(item => item.colors.length), sizes.slice(1, -1));
});

test("public reads never contact Lospec", async () => {
  const env = environment();
  await env.PALETTES.put("palettes-v1", '{"data":[],"meta":{"hasMore":false}}');
  for (let i = 0; i < 20; i++) {
    const response = await worker.fetch(new Request("https://example.com/palettes.json"), env);
    assert.equal(response.status, 200);
    assert.equal((await response.json()).data.length, 0);
  }
});

test("empty feed is unavailable without triggering an upstream fetch", async () => {
  const response = await worker.fetch(new Request("https://example.com/palettes.json"), environment());
  assert.equal(response.status, 503);
});

test("manual refresh requires a separate admin token", async () => {
  const env = environment();
  const unauthorized = await worker.fetch(new Request("https://example.com/refresh", { method: "POST" }), env);
  assert.equal(unauthorized.status, 401);
  const wrongMethod = await worker.fetch(new Request("https://example.com/refresh", {
    headers: { Authorization: `Bearer ${env.FEED_ADMIN_TOKEN}` },
  }), env);
  assert.equal(wrongMethod.status, 405);
});
