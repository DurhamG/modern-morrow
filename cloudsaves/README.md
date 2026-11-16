# Cloud saves Worker

A Cloudflare Worker that keeps one Modern Morrow save for each phrase, so a player can move a game from one device to another. The design is in [ui-rework/cloud-saves.md](../ui-rework/cloud-saves.md).

The Worker's address is `https://saves.durhamg.workers.dev`.

## Working on it

Run these in this folder.

- `npm test` runs the tests. The tests of plain functions run in Node. The tests of the requests run in Cloudflare's runtime against a local copy of R2, so they never touch the real bucket.
- `npm run check` makes the Worker's types from `wrangler.jsonc`, then type-checks the code.
- `npm run dev` runs the Worker on this machine, with a local bucket, at `http://localhost:8787`.
- `npx wrangler deploy` deploys it. CI runs the tests on every push, but doesn't deploy.

The tests use the saves in `test/saves`, which the harness made from a new game. See `test/saves.ts`.

## How it was set up

These are the steps that set up the Cloudflare account, in order, so they can be repeated.

1. **The `workers.dev` subdomain.** In the dashboard, under Workers & Pages, the account's subdomain is `durhamg`. Every Worker on the account gets an address under it, and the game's builds call this one. So it mustn't change once a build ships.
2. **R2.** Under Storage & databases, then R2 Object Storage, the checkout adds an R2 subscription. It asks for a payment method, but costs nothing within the free tier: 10 GB of storage, 1 million writes and 10 million reads a month.
3. **The tools.** In this folder:
   ```bash
   npm install
   ```
   ```bash
   npx wrangler login
   ```
   ```bash
   npx wrangler whoami
   ```
   `npm install` puts Wrangler, TypeScript, Vitest and Cloudflare's Vitest plugin in `node_modules`, at the versions in `package-lock.json`. `wrangler login` lets Wrangler manage the account, through the browser.
4. **The bucket.** It was made in the dashboard: R2 Object Storage, Create bucket, named `modern-morrow-saves`, with an automatic location and the Standard storage class. The free tier only covers Standard. `wrangler.jsonc` names the bucket.
5. **The lifecycle rule,** which deletes each file 90 days after it was written:
   ```bash
   npx wrangler r2 bucket lifecycle add modern-morrow-saves delete-after-90-days --expire-days 90
   ```
   The bucket's Settings tab in the dashboard lists it. The rule first deleted files after 365 days, and was changed to 90 days in the dashboard on 2026-11-16.
6. **The Worker:**
   ```bash
   npx wrangler deploy
   ```

## Trying the deployed Worker

From PowerShell in this folder. The `-A` gives curl a `User-Agent`, because `workers.dev` runs a browser check that can refuse requests with a library's default one.

```powershell
$url = "https://saves.durhamg.workers.dev"
```

```powershell
curl.exe -A "ModernMorrow-test" -X PUT -H "Authorization: Phrase silt%20strider" --data-binary "@test\saves\modern-morrow.omwsave" "$url/v1/save"
```

```powershell
curl.exe -A "ModernMorrow-test" -H "Authorization: Phrase silt%20strider" "$url/v1/save/info"
```

```powershell
curl.exe -A "ModernMorrow-test" -H "Authorization: Phrase silt%20strider" -o "$env:TEMP\downloaded.omwsave" "$url/v1/save"
```

`npx wrangler tail` shows the requests live. The Worker's Logs tab in the dashboard keeps each request for 3 days, with its CPU time under `$workers.cpuTimeMs`. Cloudflare hides the `Authorization` header there, so phrases aren't logged.

## CPU time

The free Workers plan allows 10 ms of CPU time a request. Reading a 15 MB upload into the Worker took 14 to 21 ms. So the Worker hands uploads straight to R2 and checks only the stored file's start and end. On 15 MB uploads, that takes 2 to 12 ms, and most are under 10. Cloudflare allows a Worker to go over now and then. If the Logs ever show requests ending with "Exceeded CPU Time Limits", which players see as error 1102, the Workers Paid plan raises the limit for $5 a month.
