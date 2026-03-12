# NPU3 Event Trace Buffer Structure

**Ring Buffer Entry (RBE)** layout used for NPU3 event trace data.

---

## One Record (RBE)

```
┌─────────────────┬──────────────────────────────────────┬─────────────────┐
│   RBE Header    │     Payload block                     │   RBE Footer    │
│   8 bytes       │     payload_words × 8 bytes          │   8 bytes       │
└─────────────────┴──────────────────────────────────────┴─────────────────┘
```

**Total size:** `8 + (payload_words × 8) + 8` bytes.

---

## RBE Header (8 bytes)

```
    0   1   2   3   4   5   6   7
  ┌───┬───────┬───┬───┬───────────┐
  │0xCA│payload_│seq│res│  reserved │
  │    │ words  │   │   │  (3 B)    │
  │    │(BE 16) │   │   │           │
  └───┴───────┴───┴───┴───────────┘
   magic   (2)   (1)(1)    (3)
```

---

## Payload Block (payload_words × 8 bytes)

```
  Payload block start
  │
  ▼
    0       4       8       12  ...
  ┌───────────────┬───────────┬─────────────────────
  │  timestamp    │ event_id  │  variable payload
  │  (8 bytes LE) │ (4 B LE)  │  (per-event args)
  └───────────────┴───────────┴─────────────────────
     event header (12 bytes)     (payload_words*8 - 12) bytes
```

---

## RBE Footer (8 bytes)

```
    0   1   2   3   4   5   6   7
  ┌───────────┬───────┬───────┬───┐
  │ reserved  │ seq   │payload_│0xBA│
  │  (3 B)    │ (2 B) │ words  │   │
  └───────────┴───────┴───────┴───┘
                              footer magic
```

---

## Full Record Layout

```
  ┌─────────────────────────────────────────────────────────────────────────────┐
  │ RBE HEADER (8 B)                                                             │
  │  [0xCA][payload_words BE][seq][res][reserved 3B]                            │
  ├─────────────────────────────────────────────────────────────────────────────┤
  │ PAYLOAD BLOCK (payload_words × 8 bytes)                                      │
  │  ┌─ Event header (12 B) ─────────────────────────────────────────────────┐  │
  │  │ [timestamp 8B LE][event_id 4B LE]                                     │  │
  │  ├─ Variable payload (event-specific, from trace_events.json) ───────────┤  │
  │  │ [arg0][arg1]...                                                        │  │
  │  └───────────────────────────────────────────────────────────────────────┘  │
  ├─────────────────────────────────────────────────────────────────────────────┤
  │ RBE FOOTER (8 B)                                                             │
  │  [reserved 3B][seq 2B][payload_words 2B][0xBA]                               │
  └─────────────────────────────────────────────────────────────────────────────┘
```
