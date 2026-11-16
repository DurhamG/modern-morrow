# Cloud saves

Cloud saves move a game from one device to another, such as from a PC to a Steam Deck. They are not a backup. A player types a phrase once on each device, and the phrase is the only key. There are no accounts.

This document lists the phases at a high level. Each phase gets a detailed plan of its own when work on it starts. Phase 1 has one, and is done.

## How players use it

- **Uploading.** The save menu has a "Cloud save" entry. It makes an ordinary local save on this device first, then uploads that same file. The upload replaces whatever the phrase held before.
- **Loading.** The load menu has a Cloud tab. It shows the one save the phrase holds: the character, level, cell and description. Choosing it downloads the save and loads it. There is no separate download step.
- **Saving afterwards.** The game's later local saves go to this device's folder for that character's name. If there is no such folder, the game makes one, the way it does for a new game. If this device already has a different playthrough under the same name, the cloud game's saves join that folder, and its quicksave and autosave slots replace that playthrough's.
- **The phrase.** The game remembers the last phrase typed on this device, so a player types it once per device. Weak phrases are allowed, because a cloud save is only a way to move a game.

## Decisions so far

- **Cloudflare Workers and R2, on the free Workers plan.** A Worker answers the game's requests and keeps each save in an R2 bucket. R2 doesn't charge for downloads, and the expected use fits in the free tiers. The cost estimate is in the original brief: about $0 to $0.50 a month for 1,000 players.
- **The Worker's address is `https://saves.durhamg.workers.dev`,** on `workers.dev`, which is free. `saves` is the Worker's name, and `durhamg` the account's subdomain. Builds call this address, so it mustn't change once one ships.
- **The Worker's code lives in this repository,** in `cloudsaves/`, so CI can test its save check against saves from the same commit's build.
- **One save per phrase.** An upload replaces the previous one. Nothing is versioned.
- **The phrase is hashed.** The bucket stores each save under a plain SHA-256 hash of the phrase, so the phrase a player typed doesn't show in the dashboard. There is no secret in the hash. The phrase isn't meant to keep anything safe, and a secret would have to be kept forever, since losing it would lose every save. The game sends the phrase in a request header, never in the URL.
- **Saves are kept for a year after their last upload.** An R2 lifecycle rule deletes each object 365 days after it was written, and every upload writes a new object.
- **Only saves from current Modern Morrow builds are accepted.** The Worker checks that the file is a save of format 37 or later that ends with Modern Morrow's `UIST` record. That record holds what the controller menus remember. The check rejects other files, saves from other OpenMW builds, and the saves at formats 42 and 43 that early Modern Morrow builds wrote. It doesn't compare tags against a list, so saves from later OpenMW formats still pass.
- **Cloudflare setup is done by hand first.** The project owner does each setup step, with instructions, to learn how it works. An API token or Cloudflare's MCP server can come later.

## Phases

### 1. Cloudflare setup and the server

The detailed plan is below, in [Phase 1 in detail](#phase-1-in-detail). [cloudsaves/README.md](../cloudsaves/README.md) records how the account was set up.

- **Done** on 2026-11-15. A save uploaded with `curl` downloads byte for byte. The deployed Worker refuses a zip, a cut-short save, an upstream save and an old format 43 save, and a refused upload leaves the phrase's save as it was. A 15 MB upload takes 2 to 12 ms of CPU time.

### 2. The game's cloud saves, on Windows

- **A small interface for the three requests,** with one implementation per platform. Windows uses WinHTTP, which ships with Windows, so the build needs no new library. Requests run off the main thread, and the result is handed back to the main loop. Every request sends a `User-Agent` such as `ModernMorrow/0.2.0`. See the note on `workers.dev` below.
- **The menus:**
  - the Cloud save entry, with its progress and its errors,
  - the Cloud tab in the load menu,
  - typing the phrase, with a controller as well as a keyboard,
  - remembering the phrase on this device, and never writing it to the log.
- **The rules as plain functions with Google Tests.** These include cleaning up the phrase and choosing the character folder for a loaded cloud save.
- **Harness scenarios** against the Worker run on this machine with `npm run dev`, which uses a local bucket. So the scenarios don't need Cloudflare, and every run of `prerelease.py` checks that the Worker accepts a save the current build wrote.
- **Done when** a game saved to the cloud on one Windows profile loads on another, and the scenarios pass.

### 3. Linux and the Steam Deck

- **An implementation of the requests for Linux,** for the AppImage. It probably uses libcurl. That phase decides whether the AppImage brings its own copy or uses the system's, and where it finds the certificates for HTTPS on SteamOS.
- **Typing the phrase in Game Mode,** where there is no keyboard. Check whether `SDL_StartTextInput` opens Steam's on-screen keyboard, and find another way if it doesn't.
- **Done when** a game moves from Windows to a Steam Deck and back.

### 4. Android

- **An implementation of the requests for Android,** through Java's built-in HTTPS client, called from the engine. The APK needs no new library.
- **Typing the phrase with a gamepad** through Android's keyboard. That's already an open question in [android.md](android.md).
- **Done when** a game moves between Android and a PC.

### 5. Launch

- The production Worker's address, in a release build.
- A short privacy note: what is stored, for how long, and that IP addresses are only used for the rate limit.
- The README section for players, and the release notes.
- Where to watch usage and cost on Cloudflare.
- Optionally, a Cloudflare API token or Cloudflare's MCP server, so an agent can manage the Worker and the bucket. CI could then deploy the Worker.

## Phase 1 in detail

### What the Worker does

All three requests carry the phrase in an `Authorization: Phrase <phrase>` header. Headers only carry ASCII safely, and players may type phrases in other alphabets, so the game sends the phrase as UTF-8 with each other byte percent-encoded, the way a URL carries it.

| Request | Answer |
|---|---|
| `PUT /v1/save`, with the save file as the body | `200` with the save's details as JSON. `400` if it isn't a current Modern Morrow save, `401` with no phrase, `413` if it's over 20 MB, `429` when rate limited. |
| `GET /v1/save` | `200` with the save file. `404` if the phrase holds none. |
| `GET /v1/save/info` | `200` with the save's details as JSON: character name, level, cell, description, content files, format, size and upload time. `404` if the phrase holds none. |

- **The phrase.** The Worker trims it, makes it lower case, and turns each run of spaces into one space. Then it hashes it with SHA-256. The hash in hex is the save's name in the bucket. A phrase must have at least one character and at most 200.
- **The size limit.** The Worker refuses a request whose `Content-Length` is over 20 MB before storing anything. One with no `Content-Length` is refused too.
- **Uploads go straight to R2.** Just reading a 15 MB upload in the Worker took 14 to 21 ms of CPU time, more than the free plan's 10 ms. So the Worker hands the request's body to R2 without reading it, and checks the stored file afterwards from two small reads: its first 256 KB and its last 64 KB.
- **The save check.** The start must hold a `TES3` header of format 37 or later and a `SAVE` record, and the Worker reads every subrecord in both. It reads the `HEDR` of format 23 and later, which holds two strings with their lengths in front. The end must be a whole `UIST` record that ends exactly where the file does. The records in between aren't read. A file with a valid start and end but anything in between passes, which costs little, because anything could already be hidden in a valid save's screenshot.
- **Each upload gets a new file name,** `<hash>/<random id>`, and a small file, `<hash>.json`, names the phrase's current save and holds its details. R2 can't rename a file, so writing `<hash>.json` is what makes a checked upload the phrase's save. A refused upload is deleted, and the phrase keeps its old save. After a good upload, the Worker deletes the previous save's file. `GET /v1/save/info` reads only `<hash>.json`. The Worker cuts long texts and long content file lists short, so it stays small.
- **The rate limit.** Workers' rate limiting binding counts requests per IP address: 10 uploads and 60 reads a minute. Its counts are approximate, and each Cloudflare location keeps its own. That's enough to stop a script from hammering the Worker.
- **Errors are short JSON,** such as `{"error": "not a save from a current Modern Morrow"}`, so phase 2 can show the player what went wrong.

### What Cloudflare's free plans allow

These limits were checked in Cloudflare's documentation in November 2026:
- [Workers](https://developers.cloudflare.com/workers/platform/limits/): 100,000 requests a day, and 10 ms of CPU time per request. Waiting for R2 doesn't count as CPU time. A request body can be 100 MB.
- [R2](https://developers.cloudflare.com/r2/pricing/): 10 GB of storage, 1 million writes and 10 million reads a month. Downloads are free. An object can only be written once a second, which is plenty for one player's save.

There is a note about `workers.dev` in [its documentation](https://developers.cloudflare.com/workers/configuration/routing/workers-dev/). Cloudflare runs a Browser Integrity Check there, which can't be turned off. It may refuse a request with error 1010 before the Worker sees it, when the `User-Agent` looks like a library's default. The game sends its own `User-Agent`, and `curl` needs `-A` in the tests.

### Setup, done by the project owner

Claude gives the instructions for each step at the time, and explains what it does. The steps are written into `cloudsaves/README.md` as they're done, so they can be repeated.

1. **Choose the account's `workers.dev` subdomain,** under Workers & Pages in the dashboard. It's in every build's address, so it shouldn't change once a build ships. A neutral name such as `modernmorrow` is best, because it's public.
2. **Turn on R2,** under Storage & databases, then R2. Cloudflare asks for a payment method even for the free tier. The project owner enters it.
3. **Install the tools.** `npm install` in `cloudsaves/` downloads Wrangler, Vitest and TypeScript into that folder only. Then `npx wrangler login` connects Wrangler to the account through the browser, and `npx wrangler whoami` shows that it worked.
4. **Make the bucket** `modern-morrow-saves`, once in the dashboard to see where buckets live, or with `npx wrangler r2 bucket create`.
5. **Add the lifecycle rule** that deletes objects 365 days after they're written, in the bucket's settings or with `npx wrangler r2 bucket lifecycle add`.
6. **Deploy the Worker** with `npx wrangler deploy`, and look at it in the dashboard.
7. **Try it with `curl`.** Upload a save, read its details, download it, and compare the two files' SHA-256. Then send the files it must refuse. `npx wrangler tail` shows the Worker's log live while this runs.
8. **Look around the dashboard:** the object in the bucket, the Worker's requests and CPU time, and R2's usage.

### The commits

1. **Add the Worker's project.** This adds `cloudsaves/` with `package.json`, `wrangler.jsonc`, `tsconfig.json`, a `.gitignore` for `node_modules` and `.wrangler`, and a Worker that answers every request with `404`. It also adds `package-lock.json` from setup step 3, so CI installs the same versions.
2. **Add the save check, with tests.** It goes in `cloudsaves/src/savecheck.ts`. Vitest tests run it on real saves in `cloudsaves/test/saves/`. They also run it on bad copies of those saves made in the test: a truncated file, padding, a `HEDR` with a wrong length, a save without `UIST`, formats 36, 42 and 43 without `UIST`, a zip, and random bytes.
3. **Add the phrase rules, with tests.** This covers cleaning up the phrase, the length limits, and the hash, with a known hash for a known phrase.
4. **Add the three requests, with tests.** The tests use the Vitest plugin's local copy of R2. They cover an upload followed by a download of the same bytes, the details, overwriting, `404` for an unknown phrase, two phrases that differ only in case and spaces finding the same save, the size limit, the missing phrase, a refused save leaving the old one in place, one save file per phrase in the bucket, and the rate limit.
5. **Run the Worker's tests in CI,** as a new job in `push.yml`. It runs `npm ci`, the TypeScript check and `npm test`.
6. **Turn on Workers Logs,** so the dashboard shows each request's CPU time.
7. **Record the setup.** This writes `cloudsaves/README.md` with the setup steps and how to deploy. It also notes the address in this document.

Setup steps 1 to 3 come before commit 1, and steps 4 to 8 after commit 4.

### Test saves

The tests need real saves, and CI has no Morrowind. So commit 2 commits a few small saves that the harness makes from a new game, at about 100 to 300 KB each. They are a save in Modern Morrow's folder and one in OpenMW's folder, at today's format 41. They also include a save from upstream OpenMW and one at the old format 43, which must both be refused. A save holds the game's state, such as the player and what changed in each cell, rather than Morrowind's own data files.

Phase 2's scenarios upload saves the game writes to the Worker running locally. So a change to the save format that breaks the check shows up before a release.

The CPU time on a large save is the one thing these saves can't show. Setup step 7 uploads a 15 MB save made by repeating a small save's records before its `UIST`. Workers Logs, turned on in `wrangler.jsonc`, shows each request's CPU time in the dashboard.
