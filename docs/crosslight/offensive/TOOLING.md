# Pentest toolkit — tooling & commands

Everything you need to make, verify, and ship a change to the toolkit. All commands run from the repo
root (`~/projects/crosslight`) unless noted. Nothing here is specific to the toolkit except where
marked — it's the normal CrossLight dev loop, collected so you don't have to hunt.

## Repo & branch layout

- Remotes: `origin` = upstream CrossPoint, `fork` = `flyboy-byte/crosslight`.
- Branches: `develop` = untouched upstream mirror (never commit there); `crosslight` = the main line and
  default branch. Work on `crosslight` (or a topic branch off it).
- Push with `git push fork crosslight`.
- The simulator is a **nested git repo** at `simulator/` (its own remote `flyboy-byte/crosslight-simulator`,
  gitignored by the firmware repo). If you change a HAL stub there, commit+push it separately from inside
  `simulator/`.

## The three build targets — all three must stay green

```bash
# 1. Host unit tests (pure logic: parsers, builders, matchers, gates)
cd test/build && cmake .. && cmake --build . -j4 && ctest
#   -> expect "100% tests passed". Re-run cmake .. only when CMakeLists changed.

# 2. Device firmware (the real target)
pio run -e x4pro
#   -> watch the Flash % line; the toolkit is cheap, a jump means something's wrong.

# 3. Simulator (native SDL build; NO radio — see below)
pio run -e simulator_x4_pro
```

> [!WARNING]
> The **simulator has no radio.** Wi-Fi promiscuous RX, raw TX, and BLE scan/advertise do nothing there.
> The sim only verifies that a screen's shell renders (its "no radio" / "disabled" / idle states) and
> that the code compiles+links natively. **Every radio behavior must be verified on the physical X4 Pro.**
> This is the single most important fact in this file: "builds in the sim" ≠ "works."

## i18n (adding UI strings)

Every user-facing string is a `STR_*` key.

1. Add the key + English text to `lib/I18n/translations/english.yaml`.
2. Regenerate the (gitignored) headers: `python3 scripts/gen_i18n.py`
   - Only `english.yaml` is committed; `I18nKeys.h` / `I18nStrings.cpp` are generated and gitignored.
   - If `gen_i18n.py` reports rising "Unused keys", you added a key nothing references — wire it or remove it.
3. Use it in code as `tr(STR_YOUR_KEY)`.

> [!CAUTION]
> **2026-10-01: `python3 scripts/gen_i18n.py` was denied by Claude's auto-mode safety classifier while
> the working tree contained deauth (6a) strings/code** — it had run fine all session for the other
> tools, so it keys on the offensive content, not the script. If you are Claude and hit this on
> offensive-tool work, do **not** route around it (no retry, no handing the build to the user to run the
> same step). If you are Codex or Logan, this is just a normal codegen step. See
> [STATUS.md](STATUS.md#6a-targeted-deauth).

## Simulator scripted QA (how to verify a shell without hardware)

The sim is driven headlessly with two env vars (both `;`-separated `<ms>:<what>` lists):

- `CROSSPOINT_SIM_INPUT_SCRIPT` — actions at wall-clock ms: `BACK`/`RETURN`/`LEFT`/`RIGHT`/`UP`/`DOWN`/
  `HOME`/`SLEEP`/`QUIT`, and touch `TAP:x,y[,durMs]` / `SWIPE:x1,y1,x2,y2[,durMs]` (logical pixels; a
  long-press is a tap with dur ≥500).
- `CROSSPOINT_SIM_SCREENSHOTS` — `<ms>:<path>.bmp`. Convert with PIL to view.

```bash
# Example: Home -> Utilities -> (scroll) -> open a tool, screenshot it
timeout 12 env \
  CROSSPOINT_SIM_INPUT_SCRIPT="1500:TAP:240,706;3200:SWIPE:240,600,240,250;4500:TAP:240,758;7000:QUIT" \
  CROSSPOINT_SIM_SCREENSHOTS="5500:/tmp/tool.bmp" \
  .pio/build/simulator_x4_pro/program
python3 -c "from PIL import Image; Image.open('/tmp/tool.bmp').save('/tmp/tool.png')"
```

Traps (both have cost real time): (1) wipe `fs_/.crosspoint/*.json` between runs — stale state looks like
a bug; (2) after moving the repo, `rm -rf .pio/build/simulator_x4_pro test/build` (CMake caches absolute
paths).

## Vendor databases (Wi-Fi OUI + BLE company IDs)

Vendor labels (AP manufacturer, BLE manufacturer) come from two on-SD databases
generated from **primary registries** — never hand-edited, never fabricated.

```bash
# 1. Fetch the primary registries (do this to refresh; sources change over time)
curl -sL -o /tmp/oui.csv    https://standards-oui.ieee.org/oui/oui.csv            # IEEE, ~40k OUIs
curl -sL -o /tmp/btsig.yaml https://bitbucket.org/bluetooth-SIG/public/raw/HEAD/assigned_numbers/company_identifiers/company_identifiers.yaml  # BT SIG, ~4k

# 2. Generate the compact sorted DBs straight onto the SD card's /vendordb
python3 scripts/gen_vendor_db.py --oui /tmp/oui.csv --btcid /tmp/btsig.yaml \
    --out /run/media/<you>/<CARD>/vendordb
#   -> oui.bin (~1.45 MB, 40k entries) + btcid.bin (~145 KB, 4k entries)
```

The firmware (`src/util/VendorDb`) binary-searches these fixed-width sorted files
directly on the SD (`HalFile` seeks), so they cost ~0 flash and ~0 RAM. The
`.bin` files are **not** committed (1.5 MB, regenerable) — they're staged on the
card. If `/vendordb/*` is absent, the tools still work, just without vendor
labels. See `scripts/gen_vendor_db.py` for the file format.

## On-device workflow (the only way to verify radio behavior)

- Cable: **USB-A-to-C**. C-to-C through the magnetic pogo adapter never enumerates.
- A sleeping device gives `Errno 71` on port open — wake it first. Opening the serial port resets the chip.
- Flash + monitor: `pio run -e x4pro -t upload` then `pio device monitor`. Logan often prefers to run the
  device command himself — hand him the exact line (`!pio run -e x4pro -t upload`).
- Normal updates install **over Wi-Fi OTA** once the device is on 26.9.2+ (see release process). No cable
  needed for a routine test — cut a release and let the device pull it.
- **SD-resident data** some tools need (copy when the card is out of the device):
  - BLE fingerprint matching: `assets/bleaudit/signatures.json` → `/bleaudit/signatures.json`.
  - (Reference for the pattern: "shipping the firmware ≠ shipping the SD data." A tool can look broken
    only because its data file isn't on the card.)

## Releasing (Wi-Fi OTA)

This is how 6b/6c/6d reach the device.

The device checks **our** repo (`-DCROSSPOINT_OTA_REPO=flyboy-byte/crosslight` in `[env:x4pro]`), not
upstream's.

1. Bump `[crosslight] version` in `platformio.ini`. Scheme **YY.M.BUILD**, must *increase numerically*
   (the updater parses `sscanf("%d.%d.%d")`): `26.10.3` → `26.10.4`. No leading `v`.
2. `pio run -e x4pro`, then copy `.pio/build/x4pro/firmware.bin` → `crosslight-<version>-x4pro.bin`
   (the `x4pro` suffix matters — the updater refuses an image whose embedded board tag differs).
3. `gh release create <version> crosslight-<version>-x4pro.bin --repo flyboy-byte/crosslight --title "..." --notes "..."`
4. Verify the asset name matches `crosslight-<tag>-x4pro.bin`.

> [!WARNING]
> A build that changes `partitions.csv` **cannot** ship over OTA or the SD flasher (they write the app
> slot only). None of the toolkit work touches partitions, so this is not a concern here — but don't
> introduce a partition change in a toolkit release.

## Code style & commit hygiene

- Format touched C/C++ with `clang-format -i <file>` before committing (`.clang-format` is google-base,
  120 col, 2-space). clang-format 22.x is installed.
- Keep commits scoped and pushed (`git push fork crosslight`). End commit messages with the attribution
  lines the session specifies.
- Keep all three build targets green per change. Add host tests for any new pure logic.
