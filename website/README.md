Modern Morrow's website
=======================

A static site: plain HTML pages that share `style.css`, with no build step. Cloudflare hosts it as a Worker with only
static assets, separate from the cloud saves Worker in `cloudsaves/`.

- `public/` is what gets published:
  - `index.html` is the home page: what Modern Morrow is, its features and the Android version.
  - `getting-started.html` explains how to install the game on each device.
  - `img/` holds the screenshots. `img/banner.jpg` is the picture behind the title on the home page, a silt strider at
    a dock near Vivec at sunset.
  - `fonts/MysticCards.ttf` is the Morrowind-style font that OpenMW ships, under the SIL Open Font License in
    `fonts/MysticCardsFontLicense.txt`. The body text uses EB Garamond from Google Fonts.
- `wrangler.jsonc` tells Cloudflare's `wrangler` tool to publish `public/`.

The textures are drawn in CSS. No art is taken from Morrowind's data files, apart from the screenshots of the game.

Previewing
----------

Serve `public/` and open http://localhost:8077:

```bash
python -m http.server 8077 --directory website/public
```

Deploying
---------

From this folder, with the `wrangler` that `cloudsaves/` installs:

```bash
../cloudsaves/node_modules/.bin/wrangler deploy
```

The first deploy asks to log in to Cloudflare, and creates the Worker `modern-morrow`. The site is then at
`https://modern-morrow.<account subdomain>.workers.dev`. A custom domain can be added to that Worker in the Cloudflare
dashboard, under the Worker's Settings, then Domains & Routes.

Cloudflare serves `getting-started.html` at `/getting-started` too, and redirects the `.html` address there.
