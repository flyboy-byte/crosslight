# E-ink firmware app/reader architecture — survey

How other e-ink reader firmwares/OSes split the **core reader** from **extra apps/tools**, and
what they document about RAM. Context for CrossLight Phase 2 (native Bible app now; networking tools
later). Researched 2026-09.

**The one thing to take away:** every system below runs on an MMU-class OS (Linux/Android), so their
"apps" are either **in-process plugins** (KOReader) or **separate processes/binaries under a launcher**
(InkBox, Onyx). CrossPoint is bare-metal ESP32 — no OS, no processes, no dynamic loading — so the only
model that physically transfers is KOReader's **"self-register into a menu/dispatcher, run in the one
process, one screen at a time"**, minus the dynamic Lua loading. What CrossPoint already does (Activity
stack, `HomeMenuItem`) *is* that model, hand-rolled.

---

## KOReader (Lua on Kobo/Kindle/Android/Linux) — closest analog

| Aspect | How it works |
| --- | --- |
| Reader vs app split | A plugin is a folder `*.koplugin/` with `main.lua` whose class extends `WidgetContainer`/`Widget`. `PluginLoader` discovers them at startup from `plugins/` + `extra_plugin_paths`. Plugins **run in the main process**, not separate binaries. |
| How apps register | Plugin hooks itself into the **main menu** and/or the **Dispatcher** (action/gesture registry). It does not touch the render engine directly; cross-plugin data goes through a `PluginShare` singleton table. Errors are wrapped in a `HandlerSandbox` so one plugin can't crash the app. |
| Non-reading tools | SSH (`dropbear`), NewsDownloader (RSS/Atom), KeepAlive, terminal, calculator — all just system-level plugins registered into menus. Same mechanism as reading-adjacent plugins. |
| Resource notes | Plugins can be turned off via `plugins_disabled` **to conserve resources** — loading is opt-out. Real-world RAM: ~50 MB at start, climbing to ~140 MB after heavy use (manga/CBZ, big fonts); low-end devices OOM. Doc'd fix is scaling back `DGLOBAL_CACHE_SIZE_MAXIMUM` — i.e. **the cache is the RAM sink, not the plugin count**. |

Sources: [Plugin System (DeepWiki)](https://deepwiki.com/koreader/koreader/9-plugin-system-and-extensions),
[hello.koplugin](https://github.com/koreader/koreader/blob/master/plugins/hello.koplugin/main.lua),
[OOM issue #13998](https://github.com/koreader/koreader/issues/13998),
[RAM issue #5998](https://github.com/koreader/koreader/issues/5998).

**Transferable:** (1) A plugin *registers itself into a menu/dispatcher* rather than the home screen
hard-coding it — this is exactly the declarative `App`/`AppRegistry` shape worth adopting over the
hand-rolled `HomeMenuItem` enum. (2) Their scaling pain is **caches, not features** — matches
CrossLight's picture (the Bible app is ~31 KB; the reader's section/glyph caches are the RAM story).

---

## InkBox OS / Quill (Qt5 on Kobo, Alpine Linux) — separate-process model

| Aspect | How it works |
| --- | --- |
| Reader vs app split | Full standalone OS on **Alpine Linux 3.10**. The "InkBox shell" is a launcher; apps are **separate binaries/processes**, some preinstalled, some downloadable. A KoBox X11 subsystem runs arbitrary X11/SDL apps from the Alpine repo. |
| How apps launch | Shell **functions as an app launcher**; reader is just one app among many (muPDF-backed EPUB/PDF, plus NetSurf browser, etc.). |
| Resource notes | Not quantified in public docs. Enforces **only signed software runs** (security policy), and uses EncFS encrypted storage. |

Sources: [Quill repo](https://github.com/Quill-OS/quill),
[InkBox 2.0 writeup](https://www.debugpoint.com/inkbox-os-2-0/),
[HN thread](https://news.ycombinator.com/item?id=36495354).

**Transferable:** almost nothing structurally (needs an MMU + filesystem of executables CrossPoint
doesn't have). Worth noting only as the "real OS" end of the spectrum: signed-app + launcher is what
you get *once you have Linux*, and it's a reminder that CrossLight's single-image model is the correct
one for the hardware, not a limitation to engineer around.

---

## Others (one line each)

- **Onyx Boox** — full **Android**; "apps" are ordinary APKs, each its own process/sandbox, Play Store
  preinstalled. Maximum flexibility, maximum RAM/battery cost; the opposite pole from a microcontroller.
  [source](https://www.androidpolice.com/best-boox-e-readers-tablets/)
- **Plato** (Rust, Kobo) — monolithic single-binary reader; "apps" are built-in *views* inside one
  program, not a plugin system. Closest in spirit to CrossPoint's "one binary, subclass the base"
  approach, just without a registry.

---

## Takeaways for CrossLight

1. **The model I'd copy is KOReader's registration seam, not its runtime.** Have modules *register*
   themselves (menu entry + action) into a table the home screen reads, instead of the home menu
   enumerating them by hand. That's the `App`/`AppRegistry` shape from `crosspoint-reader-apps`;
   reimplement with function pointers, not `std::function` (AGENTS.md render-path rule).
2. **No firmware here loads apps dynamically on comparable hardware — they either have an OS or they
   don't.** CrossLight has no OS, so "modules" = statically-linked activities chosen at build time.
   That's not a compromise; it's the same thing Plato does and what the ESP32 supports.
3. **RAM scaling is dominated by caches and the radio stack, never by how many features are compiled
   in.** KOReader's OOMs are cache-driven; CrossLight's Bible app costs ~0 runtime RAM. The real Phase-2
   RAM question is the WiFi/BLE stack's working set while a networking tool is the (single) active
   activity — not "reader + tool" coexisting, because they never do.
4. **"Turn it off to save resources" is a real pattern** (KOReader `plugins_disabled`). If networking
   tools ever bloat the image or idle cost, a build-flag/capability gate (CrossPoint already uses
   `FREEINK_CAP_*`) is the sanctioned equivalent.
