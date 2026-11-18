# Shared Lospec feed

This Cloudflare Worker serves `/palettes.json` to both Zero and Steam builds.
The app needs only that public HTTPS URL. Store your Lospec key in the Worker,
not in a distributed app.

A scheduled refresh runs every six hours, checking up to three pages of 100
newest palettes from Lospec. That is at most 12 Lospec requests per day,
plus any admin refreshes. The Worker keeps 4-, 8-, and 16-color palettes,
preserving names, creators, colors, likes, and newest-first order. Likes are
filtered on the device, so changing the threshold does not call Lospec.
Public requests read a shared KV snapshot and never call Lospec. If any
upstream page fails, the previous snapshot stays available.

## Deploy to your Cloudflare account

From this folder, using a Node.js version supported by Wrangler:

```sh
npx wrangler login
npx wrangler kv namespace create PALETTES
```

Replace the placeholder `id` in `wrangler.toml` with the returned namespace
ID. Then deploy and enter the secrets at the prompts:

```sh
npx wrangler deploy
npx wrangler secret put LOSPEC_API_KEY
npx wrangler secret put FEED_ADMIN_TOKEN
```

`LOSPEC_API_KEY` is your Lospec key. `FEED_ADMIN_TOKEN` should be a separate,
long random password you keep for manual refreshes. Neither belongs in the
repository, app, feed URL, or command arguments. For local Worker testing,
an ignored `.dev.vars` file can hold them.

The Worker URL will be printed by deploy. For example, the app's endpoint
would be `https://bitmap16dx-palettes.YOUR-SUBDOMAIN.workers.dev/palettes.json`.
A custom Cloudflare domain can point to the same Worker.

To populate the initial feed without waiting for the schedule, send a `POST`
to `/refresh` with `Authorization: Bearer <FEED_ADMIN_TOKEN>` using an HTTP
client that stores the token privately. The public endpoint returns 503
until the first successful refresh. Only the admin endpoint calls Lospec;
public requests cannot trigger a refresh. The admin token is never sent to
Lospec. The palette feed response contains no credentials.

KV updates can take about a minute to become visible in another location.
Public responses allow 15 minutes of HTTP caching. Devices keep their own
last successful results across launches.

## Build the app

From the repository root:

```sh
cmake --preset zero \
  -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
cmake --build --preset zero

cmake --preset steamdeck \
  -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
cmake --build --preset steamdeck
```

For Zero device packaging from a Mac:

```sh
BITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json \
  ./platform_zero/device.sh package pi@cardputerzero.local
```

When a feed URL is configured, the build excludes any local release key.
The direct API mode remains available for development without hosting.

## Verify locally

```sh
node --test services/lospec-feed/worker.test.mjs
ctest --test-dir build_zero --output-on-failure
```

The Worker tests use simulated Lospec responses and KV storage. They verify
request bounds, secret handling, palette validation, preserved snapshots
after errors, and public reads that never access Lospec. Deployment and a
live fetch require your Cloudflare account and Lospec credential.

References: [Lospec API](https://api.lospec.com/docs/),
[Cloudflare secrets](https://developers.cloudflare.com/workers/configuration/secrets/),
[Cron Triggers](https://developers.cloudflare.com/workers/configuration/cron-triggers/),
[KV reads](https://developers.cloudflare.com/kv/api/read-key-value-pairs/).
