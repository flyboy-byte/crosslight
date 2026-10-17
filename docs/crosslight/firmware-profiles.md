# Firmware profiles & build flavors (SCOPE — nothing built yet)

Status: scoping started 2026-10-17, prompted by two things landing at once — the unified-API
proposal (`docs/crosslight/unified-api.md`) adding a new personal-integration surface, and the
2026-10-12 backlog idea (`PLAN.md` "Backlog") of cutting the firmware into flavors instead of one
fixed build. This doc turns that backlog line into an actual plan. **Nothing in this doc is built.**
It exists so the next session (or Logan) can start Phase 1 without re-deriving the design.

**The repo stays one public MIT tree.** This is not a public/private fork split. Every module —
including the Claude integrations, the Libre glucose bridge, the BLE relay, and whatever the
unified API becomes — stays in this one open repository, in the public build, same as today's
`/claude/token.txt` / `/libre/url.txt` pattern. What changes is making it *obvious and easy* for
someone who isn't Logan to end up with a build that doesn't carry those modules at all, or that
carries them inert and clearly documented as "bring your own account/server." (Logan's own words:
"id like it all public, with the more personal stuff still in here and documented but obvious on
how to avoid it in the docs and releases.")

## The two mechanisms, and why both

Logan's call: build **both** layers rather than picking one.

1. **Compile-time flavor (PlatformIO envs).** What CAN exist in a given `.bin` at all — a flash
   and personal/public boundary. A `bible`-flavored env never links `ClaudePanelActivity`,
   `GlucoseMonitorActivity`, `LibreBleServer`, or the offensive tree; that code isn't in the
   binary to find or enable. This is the same tool already used for `-DOMIT_FONTS` on `x4pro`
   (`platformio.ini:403`) and for the simulator split (`env:x4pro` vs `env:simulator_x4_pro`) —
   not a new mechanism, an extension of one already load-bearing in this tree.
2. **Runtime profile (SD config).** What's actually enabled/visible within whatever got compiled
   in — no reflash needed. This already exists in miniature: `UtilityRegistry.cpp`'s
   `kOffensiveFlag` (`/offensive/enabled`) hides the whole offensive tool list unless that file is
   present, and `Utility::available()` (`UtilityRegistry.h:35`) is already a per-tool runtime gate
   hook, currently unused by any registered tool but exactly the mechanism a profile system would
   drive. Generalizing this from one boolean flag to a named profile is the bulk of Phase 1.

Compile-time answers "is the code even in this image" (flash budget, and the honest line between
"public anyone can build" and "needs Logan's specific hardware/account to do anything"). Runtime
answers "what does this specific device show today" (so a flavor that includes everything can
still be dialed down to Bible-only without a reflash, and so Logan can personalize his own device
without that becoming a second firmware fork).

## Module tagging (read from the current registry, not invented)

Every entry in `UtilityRegistry.cpp`'s `kGeneral`/`kOffensive`, plus the Bible app and the two
`HomeMenuItem` reader paths, falls into one of three kinds. This tagging is the thing both
mechanisms key off of — it doesn't exist as a type yet, see "What Phase 1 actually adds" below.

| Kind | Meaning | Current modules |
| --- | --- | --- |
| `Generic` | Useful to literally anyone who flashes this firmware, no account/server/third-party dependency | Reader (`FILE_BROWSER`/`LIBRARY`/`OPDS_BROWSER`), Bible app, Calculator, Flashlight, Unit Converter, Wi-Fi Analyzer, Bluetooth Scanner |
| `PersonalIntegration` | Code is generic and config-driven (SD file, same shape as `/claude/token.txt`), but does nothing until someone points it at their own account/server/hardware — not Logan-specific, just currently only Logan has the other end of the wire | Claude Panel, Ask Claude, Bible Passage Q&A, Glucose Monitor (`LibreBridgeClient`), BLE glucose relay (`LibreBleServer`), the unified-API bridge client once it exists |
| `OffensiveAudit` | Already its own gated tree, scope is the operator's own gear, existing SD-flag precedent (`docs/crosslight/offensive/STATUS.md`) | Camera Scan, Wi-Fi Threat, Wi-Fi Capture, PMKID Harvest, Beacon Flood, Evil Twin, BLE Spoof |

`PersonalIntegration` is the kind the unified API grows. Logan's answer on it specifically:
**included but unconfigured** in every flavor, same as `/claude/token.txt` today — the public
build ships the generic bridge client, pointed at nothing, so anyone could stand up their own
compatible server and use it; it is not compiled out of the public tree and not hardcoded to
Logan's own deployment.

## What Phase 1 actually adds (runtime profile, no reflash)

- A `ModuleKind` tag (the three rows above) on `utilities::Utility`, and the equivalent for the
  two non-Utilities `HomeMenuItem`s that aren't always-on (`BIBLE`, `UTILITIES` itself — the
  reader/library/settings entries stay always-on in every profile, there's no version of this
  firmware that can't read a book).
- A profile file read once at boot, same pattern as `kOffensiveFlag` (cached, read once, needs a
  reboot to change) — exact path/format is an open question below, but the shape is "a short SD
  text/JSON file naming which kinds are enabled," defaulting to **everything enabled** when the
  file is absent so an existing device's behavior doesn't change under it with no action taken.
- `UtilityRegistry::resolved()` filters by tag + profile instead of just the one offensive
  boolean; `Utility::available()` stays as the finer per-tool hook it already is (e.g. a
  `PersonalIntegration` tool whose SD config file is actually present could still be hidden by
  profile, independent of whether it would otherwise work).
- Home's `BIBLE`/`UTILITIES` entries: if a profile drops every entry in `kGeneral` for Utilities,
  or Bible is excluded by profile, that `HomeMenuItem` row itself should disappear rather than open
  onto an empty list — needs a look at `HomeActivity`'s hand-maintained index math (see
  `PLAN.md` "Architecture notes") before this is more than a sentence.

## What Phase 2 actually adds (compile-time flavor, needs a reflash)

- New `platformio.ini` environments alongside `x4pro`/`simulator_x4_pro` — naming and exact count
  still open (see questions below), but the shape is one env per flavor, each defining a
  `-DCROSSLIGHT_FLAVOR_*` macro that `UtilityRegistry.cpp` and `main.cpp`'s activity registration
  wrap non-`Generic` modules in, so a `bible`-flavored `.bin` never links `ClaudePanelActivity` /
  `GlucoseMonitorActivity` / `LibreBleServer` / the offensive tree at all — not hidden, not present.
- `OMIT_FONTS`-style discipline applies here too: a flavor that drops a UI screen needs either an
  on-device check or a simulator build with the same flags, per the exact process note PLAN.md
  already wrote down after the 26.10.13 font bug. This is not new risk, just the existing one
  showing up in a second place.
- Release pipeline: today's GitHub Releases ship one `.bin`. Flavors mean naming and publishing
  more than one artifact per version tag — exact mechanics (asset naming, which flavors get a
  release every time vs. on request) are an open question, not decided here.

## Docs impact (the "obvious how to avoid it" half of the ask)

- A new top-level README section (or a linked doc) listing, plainly: which features need you to
  bring your own account/server/hardware, exactly which SD file each one reads, and what happens
  if you never create that file (nothing — the tool sits inert, same as today). This is really
  just writing down, in one place, a pattern that already exists per-feature
  (`claude-panel.md`, `claude-features.md`, the Glucose Monitor section of this PLAN.md) rather
  than inventing new behavior.
- Each flavor needs a one-paragraph "what you get and don't" in the README build table, and the
  runtime profile file needs its own short doc (likely folded into this one once Phase 1 ships)
  the same way `/offensive/enabled` has `docs/crosslight/offensive/STATUS.md`.

## Open questions for Logan

1. **Profile file format/location** — a single value (`/profile.txt` containing `full`/`bible`/
   `minimal`) or a per-kind toggle file? Single value is simpler; per-kind is more flexible if a
   profile ever needs something odd like "Bible + offensive, no Claude."
2. **Flavor set for Phase 2** — how many `platformio.ini` envs, and what's in each? A starting
   guess, not a decision: `full` (everything, today's `x4pro` behavior), `bible` (reader + Bible
   app only, nothing else compiled), and `reader` (reader + generic utilities, no
   `PersonalIntegration` or `OffensiveAudit` code) — three, not the four from the original backlog
   note, since `OffensiveAudit` already has its own runtime gate and doesn't obviously need a
   fourth binary. Confirm or redraw this.
3. **Is a runtime profile switch exposed in Settings UI**, or SD-file-only like `/offensive/enabled`
   (no on-device UI, edit the card)? SD-file-only is less work and matches precedent; a Settings
   toggle is friendlier for a build Logan hands to someone else.
4. **Release artifact naming** for Phase 2 — this needs an answer before the first multi-flavor
   release, not before Phase 1 ships (Phase 1 doesn't touch the release pipeline at all).

## Phase gates

| Phase | Exit condition |
| --- | --- |
| 0 — this doc | Logan confirms the shape (or redraws it) |
| 1 — runtime profile | `UtilityRegistry` filters by profile; a real device set to a reduced profile shows the reduced menu; default (no file) is provably identical to today's behavior |
| 2 — compile-time flavor | At least one non-`full` `platformio.ini` env builds clean, boots on real hardware, and measurably uses less flash than `full` |
| 3 — unified API as a `PersonalIntegration` module | Generic bridge client lands, unconfigured in every flavor, following this doc's tagging — scoped separately once the server side (`extraction` repo) has real routes to point at, per `docs/crosslight/unified-api.md`'s own phase table |
| 4 — docs/release pipeline | README build table lists flavors; multi-artifact release actually cut once |

Related: `PLAN.md` "Backlog (queued 2026-10-01)", `docs/crosslight/unified-api.md`,
`src/utilities/UtilityRegistry.{h,cpp}`, `docs/crosslight/offensive/STATUS.md`.
