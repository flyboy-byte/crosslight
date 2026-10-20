# Claude relay over BLE (v1 AGREED with extraction 2026-10-19, nothing built yet)

Status: **format confirmed by extraction (with the four changes folded in below); reader and phone sides not built.** Goal (Logan): Ask Claude, Bible Passage Q&A and the Claude
usage glance work with the reader's Wi-Fi off, the phone's companion app doing the bridge call.
Evidence tags: facts about the app/server are extraction's (2026-10-19 message), DOCUMENTED = in
`docs/crosslight/unified-api.md`; throughput/stability over BLE is UNKNOWN until measured.

## Roles (forced by BLE)
The phone is the GATT central; the reader is the peripheral. The phone cannot receive a write from the reader, so:
- **Reader -> phone**: NOTIFY on the *relay TX* characteristic (the app subscribes to its CCCD).
- **Phone -> reader**: WRITE / WRITE_NR on the *relay RX* characteristic.
Both live in the existing service `0c0cd14d-6582-46f7-91b6-b4cf2a7cebac`. Existing glucose and time characteristics are unchanged.

| Name | UUID | Properties |
| --- | --- | --- |
| Relay TX (reader -> phone) | `a65f2d9a-0d09-4190-a32b-63ba6c91daac` | NOTIFY |
| Relay RX (phone -> reader) | `b9e7080d-9fd5-4a71-ad1e-67186b5b2d9d` | WRITE, WRITE_NR |

The phone should request an MTU (247+, up to 517) right after connecting and chunk by `MTU - 3`. Default MTU 23
works but needs ~250 frames for a 4000-char reply; treat it as a fallback only.

## Frame
All multi-byte integers little-endian. UTF-8 text.

```
byte 0  type     1 = prompt, 2 = usage, 4 = cancel          (replies echo the request's type)
byte 1  id       request id 1..255, chosen by the reader, echoed in EVERY reply frame
byte 2  status   requests: 0.  replies: 0 ok, 1 forbidden (403), 2 busy/rate limited (429), 3 timeout,
                 4 server error (5xx/502/503), 5 bad request (400/413/415), 6 not configured (no key),
                 7 unauthorized (401), 255 other
byte 3  flags    bit0 MORE (another frame of this message follows); bit1 TRUNCATED (prompt reply was cut at a UTF-8
                 boundary at the 4000-byte cap; set on the FIRST frame); other bits 0
FIRST frame of a message only:  bytes 4-5  total payload length (u16)  then payload bytes
CONTINUATION frames:            payload bytes follow byte 3 directly (no length)
```
A message ends when MORE is clear. The phone checks the declared total against what it sent/received; a mismatch aborts
that request with status 5 and partial text is never shown as a reply. The reader does the same on receive. A reply for a stale/unknown `id` is dropped. One request in flight at a time
(the server enforces the same); a new request while one is open is refused with status 2.

## Messages
- **prompt (1), request**: payload = prompt text, <= 4000 BYTES (the server cap is 4000 chars; bytes is the safer bound). **reply**: status 0 + reply text <= 4000
  bytes, else a non-zero status with an empty payload (status text is the reader's to choose).
- **usage (2), request**: empty payload. **reply** (status 0), fixed 16 bytes, no JSON:
  `u8 5h_util_pct (0-100 whole percent, 255 unknown/null)  u32 5h_reset_epoch (unix UTC, 0 unknown)  u8 5h_status (always 255: the API has no per-window state)`
  `u8 7d_util_pct  u32 7d_reset_epoch  u8 7d_status (255)`  `u32 age_s (seconds since the server last had real usage numbers, 0xFFFFFFFF unknown)`.
  The server's single overall state string is not carried in v1. The reader treats usage older than 10 minutes (and 0xFFFFFFFF)
  like old glucose: "unknown", never a number. Whether the server can produce 5h/7d numbers at all is UNKNOWN today; the reader
  must handle 255 everywhere.
- **cancel (4)**: reader -> phone, `id` of the request to abandon; no payload, no reply.
- Server status (`/api/v1/status`): reserved, not in v1.
- Glucose never travels over this channel (it has its own characteristic).

## Behaviour
- The reader advertises while a relay-using screen is open (as Glucose Monitor does), connects, sends one request frame
  sequence, waits for the reply, and shows it. Per-request timeout on the reader: 60 s (the phone's HTTP call may take up to 45 s).
- The phone answers with a status frame on any failure rather than leaving the reader waiting. On a 429 it carries
  `retryAfter` as ASCII seconds in the payload only if the server sent one (unverified); otherwise the payload is empty and the
  reader assumes the 10 s gate gap.
- Server limits bind the reader too: 4000 chars, one at a time, 10 s gap, 100/day, 40/day per key. Never retry in a loop.
- Display rules unchanged: no interpretation of glucose, no glucose through this channel.

## Open questions
- Protocol version byte: decided NO. An incompatible change means new characteristic UUIDs.
- Whether the reader should prefer Wi-Fi when both are available. Proposed: BLE first when a phone is connected, Wi-Fi as fallback.

## Security note (flagged by extraction, accepted)
The link is unencrypted (decided 2026-10-18 for glucose). For the relay that means anyone in range could read replies or
write a request to RX and spend the daily quota. The phone only acts on requests arriving over the connection the app
opened. The reader only advertises while a relay screen is open. Revisit if this is not acceptable.

## Phone-side sequence (extraction)
connect -> discover services -> requestMtu(517), wait onMtuChanged -> enable notifications on TX (write the CCCD, wait for the
callback) -> write time -> write glucose. All strictly sequential (Android allows one GATT op at a time). The reader must not
notify a request before it has seen the CCCD enable.
