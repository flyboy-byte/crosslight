# CrossLight offensive / radio-audit tools — paused & quarantined

This folder documents the **offensive radio tools**, which are on a deliberate **hard pause** as of
2026-10-04. They were split out of the everyday firmware and walled off so CrossLight's user-facing
feature set is just a clean e-reader plus two plain radio utilities. This folder is the reference for
anyone (Codex, Claude, or Logan) who later wants to understand, re-enable, or resume them — it is *not*
an active work plan. The rest of CrossLight (reader, Bible app, general utilities) lives in the repo
root `PLAN.md`.

> [!IMPORTANT]
> **Decision (2026-10-04).** Stop active development of the Wi-Fi/BLE *hacking* tools. Keep the two
> genuinely general-use tools — **Wi-Fi Analyzer** and **Bluetooth Scanner** — in the normal Utilities
> menu. Move everything else (transmit, impersonate, credential-capture, surveillance-detect) into
> `src/offensive/`, keep it compiled but **hidden from the menu**, and forget about it unless explicitly
> revisited. This folder is the "if you come back to it" map.

## What is where now

| Layer | Lives in | In the menu? |
|-------|----------|--------------|
| **General utilities** — Wi-Fi Analyzer (AP scan), Bluetooth Scanner (BLE scan), both with vendor labels | `src/activities/utilities/` + `src/wifiaudit/` (ApScanner, WifiFrame) + `src/bleaudit/` (BleScanner, BleAd, BleSignatures) + `src/util/VendorDb` | **Yes, always** |
| **Offensive / active-audit** — beacon flood, evil twin, BLE spoof, PCAP capture, PMKID harvest, Wi-Fi threat detection, Flock/camera detector | `src/offensive/` (engines + `activities/` + `flock/`) | **No** — hidden unless the SD flag is present |
| **Targeted deauth (6a)** — the one purely-disruptive tool | `git stash` only, never materialized | No — not even in the tree |

## How the hidden tools are gated

Two independent layers, both still in place:

1. **SD flag file `/offensive/enabled`.** The utility registry (`src/utilities/UtilityRegistry.cpp`)
   reads this once per boot. Present → the offensive tiles appear in Utilities. Absent → clean menu.
   No rebuild needed either way; create the (empty) file and reboot to reveal them, delete it to hide.
2. **Compile flag `CROSSLIGHT_ENABLE_ACTIVE_AUDIT` + per-boot `ActiveAuditGate`.** Unchanged from
   before — the transmit edge (`AttackTx`) still refuses unless the build defines the flag (it does, in
   `[env:x4pro]`) and the per-boot gate is confirmed. This is defense-in-depth *on top of* the menu
   gating. See [ARCHITECTURE.md](ARCHITECTURE.md#the-gating-model).

## Authorization & scope (unchanged, non-negotiable)

These tools are for **Logan's own hardware** — his own laptops/APs/BLE devices, personal
security-research context (Extra-class ham, understands RF law). The hard boundary: **point it at gear
you own, never shared/dorm/university network infrastructure or other people's devices.** That boundary
is about *whose network it is*, not competence.

## If you're resuming this (cold start)

Read in this order — but remember the default state is "paused," so these describe a frozen system:

| # | Doc | What it gives you |
|---|-----|-------------------|
| 1 | **[STATUS.md](STATUS.md)** | Per-tool state: built / simulator-checked / hardware-tested / open, as of the pause. |
| 2 | **[ARCHITECTURE.md](ARCHITECTURE.md)** | How it's wired now (`src/offensive/` layout), the gating model, the patterns. |
| 3 | **[TOOLING.md](TOOLING.md)** | Exact build / host-test / simulator / flash / release commands. |
| 4 | **[ROADMAP.md](ROADMAP.md)** | The open work that *was* queued, as task specs — only relevant if the pause is lifted. |

## One-paragraph state (2026-10-04)

Two general radio utilities (Wi-Fi Analyzer, Bluetooth Scanner) are **shipped and hardware-verified**
(BLE scan + full vendor labeling confirmed on device 2026-10-03; Wi-Fi scan fixed in 26.10.6 — see the
`WIFI_MODE_STA` promiscuous-init fix). The offensive tools (beacon flood `6b`, evil twin `6c`, BLE spoof
`6d`, plus PCAP capture, PMKID harvest, threat detection, Flock detector) are **compiled but hidden**,
and were never fully hardware-verified before the pause — if resumed, a flash-test of the active tools
is the first owed step. Targeted deauth `6a` remains **in a git stash, never compiled** (Claude's safety
tooling blocked the build; it was never routed around). See [STATUS.md](STATUS.md).
