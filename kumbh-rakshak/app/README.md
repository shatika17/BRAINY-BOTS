# app/

Files for the installable web app (PWA):

| File | What it is |
|---|---|
| `index.html` | The app itself (single file). Add your AI Studio / km.html code here. |
| `sw.js` | Service worker: caches the app so it opens in airplane mode |
| `manifest.json` | PWA manifest: name, colours, icons |
| `icon-192.png`, `icon-512.png` | App icons |

In `index.html`, register the worker with `navigator.serviceWorker.register('./sw.js')` and link the manifest with
`<link rel="manifest" href="manifest.json">`. The service worker only runs over HTTPS (for example GitHub Pages or Netlify),
not from a double-clicked file.
