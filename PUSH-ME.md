# Ship Musicaguide via GitHub Actions

This repo builds **macOS (VST3 + AU + Standalone, universal)** and **Windows (VST3 + Standalone)**
automatically on every push. No code signing / notarization yet (you chose to skip it for now),
so the artifacts are unsigned — fine for testing and early users who allow unsigned plugins.

## Security notes (already handled)
- **No secrets in this repo.** Users supply their own AI API key at runtime; it's stored in the OS
  secure store (macOS Keychain / Windows Credential Manager), never committed, never logged.
- The only key in the source is a Supabase **publishable** key (`sb_publishable_...`) with insert-only
  RLS — it is *designed* to be public (like a Firebase web config). Safe to commit.
- The workflow uses pinned action versions and `permissions: contents: read` (least privilege).

## Push it (run from this folder)
Option A — GitHub CLI (easiest, handles auth securely):
    gh auth login          # one time, opens a browser/device login
    gh repo create musicaguide --private --source=. --push

Option B — plain git (use a Personal Access Token when prompted, not your password):
    git remote add origin https://github.com/<your-username>/musicaguide.git
    git push -u origin main

## After pushing
- Open the repo on github.com → **Actions** tab → watch the `build` workflow run (~10–20 min).
- When it's green, open the run → **Artifacts** → download `Musicaguide-macOS` / `Musicaguide-Windows`.
- Those zips contain the installable plugin folders. Link them from your site's download button
  (or attach them to a GitHub **Release** so the download is a stable URL).
