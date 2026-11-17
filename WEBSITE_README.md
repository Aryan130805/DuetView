# DuetView Official Product Website

This directory contains the official website for **DuetView** (companion display solution for Windows & Android), inspired by modern product websites like [Campus Saathi](https://campussathii.netlify.app/).

---

## 🌟 Highlights & Architecture

- **Zero-Dependency Architecture:** Pure semantic HTML5, modern vanilla CSS3 (CSS custom properties, glassmorphism, responsive flex/grid), and lightweight vanilla ES6 JavaScript. No Node.js build step or bulky runtime dependencies required.
- **Campus Saathi Inspiration:** Sleek dark navy palette (`#080C15`, `#121B2F`, `#18243E`), cyan accents (`#38BDF8`), purple/blue gradient typography, interactive dual-display device mockup, telemetry metric stats, modular feature cards, step-by-step roadmap, and accessible FAQ accordion.
- **Privacy Policy Integration:** Fully integrates the official, zero-cloud peer-to-peer [`privacy.html`](privacy.html) without altering legal text, dates, or permission justifications.
- **Dedicated Terms & Conditions:** Full MIT license and user guidelines on [`terms.html`](terms.html).
- **Dual-Hosting Parity:** All files are structured at root (for Netlify/Vercel) and mirrored into `/docs` (for GitHub Pages `docs/` deployment).

---

## 📂 Directory Structure

```text
DuetView/
├── index.html                   # Primary landing page (All 13 core sections)
├── privacy.html                 # Official DuetView Privacy Policy (Preserved)
├── terms.html                   # Terms & Conditions (MIT License & system requirements)
├── _redirects                   # Netlify clean URL rewrites (/privacy-policy -> /privacy.html)
├── netlify.toml                 # Netlify deployment settings & HTTP security headers
├── robots.txt                   # Search crawler directives
├── sitemap.xml                  # Canonical XML sitemap for Google & Bing
├── favicon.png                  # High-resolution website favicon
├── assets/
│   └── website/
│       ├── css/
│       │   └── style.css        # Complete responsive stylesheet & design system
│       ├── js/
│       │   └── main.js          # Hamburger menu, FAQ accordion, tabs & clipboard copy
│       └── images/
│           ├── duetview_icon.png # Official 512x512 app icon
│           └── favicon.png       # 32x32 / 48x48 icon
└── docs/                        # Synchronized mirror for GitHub Pages
    ├── index.html
    ├── privacy.html
    ├── terms.html
    ├── _redirects
    ├── robots.txt
    ├── sitemap.xml
    └── assets/
        └── website/...
```

---

## 🚀 How to Preview Locally

You can test the website locally using any standard HTTP server:

### Option 1 — Python 3 (Recommended)
```bash
python -m http.server 8000
```
Then visit [http://localhost:8000](http://localhost:8000) in your browser.

### Option 2 — VS Code Live Server
Right-click `index.html` inside VS Code / Antigravity and select **"Open with Live Server"**.

---

## 🌐 Deployment Instructions

### 1. Deploying to Netlify (Fastest)

#### Method A: Direct Git Deployment (Recommended)
1. Go to [Netlify App](https://app.netlify.com/) and click **"Add new site" &rarr; "Import an existing project"**.
2. Connect your GitHub account and select the `DuetView` repository.
3. Configure the build settings:
   - **Base directory:** (leave empty)
   - **Build command:** (leave empty)
   - **Publish directory:** `.` (root)
4. Click **Deploy DuetView**. Netlify will automatically detect `netlify.toml` and `_redirects`.

#### Method B: Netlify CLI
```bash
npm install -g netlify-cli
netlify deploy --prod --dir=.
```

---

### 2. Deploying to GitHub Pages

1. In your GitHub repository (`Aryan130805/DuetView`), go to **Settings &rarr; Pages**.
2. Under **Build and deployment &rarr; Source**, choose **Deploy from a branch**.
3. Under **Branch**, select `main` and folder `/docs`.
4. Click **Save**. Your site will be published at `https://aryan130805.github.io/DuetView/`.

---

## 🔗 Customizing Release Links and Checksums

When publishing a new release (e.g. `v0.5.0`):

1. Open `index.html` and search for `v0.4.0`.
2. Update the version badges and the direct download links:
   - Android APK: `https://github.com/Aryan130805/DuetView/releases/download/v0.5.0/DuetView-Android.apk`
   - Windows Installer: `https://github.com/Aryan130805/DuetView/releases/download/v0.5.0/DuetView-PC-Setup.exe`
   - Windows Portable: `https://github.com/Aryan130805/DuetView/releases/download/v0.5.0/DuetView-PC-0.5.0-win64.zip`
3. Update the SHA-256 strings in the **Release Asset Cryptographic Checksums** box:
   - `id="hash-apk"` and its matching `data-checksum="..."` attribute
   - `id="hash-setup"` and its matching `data-checksum="..."` attribute
   - `id="hash-zip"` and its matching `data-checksum="..."` attribute
4. Copy the changes to `docs/index.html`.

---

## 🛡️ Privacy & Compliance Notes

The website prominently displays:
- **Zero-Cloud Architecture:** Highlighting that video pixels, touch inputs, and credentials never touch external servers.
- **Privacy Policy:** Readily accessible in header navigation, footer, and via clean URLs (`/privacy-policy` and `/privacy`).
- **Open Source Transparency:** Clear links to the GitHub repository, issue tracker, and MIT License.
