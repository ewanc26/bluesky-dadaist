# AGENTS.md

Guidance for agents working on `bluesky-dadaist`, a C23 Dadaist-oracle Bluesky bot
that builds a word-level markov chain from the live AT Protocol firehose and
replies to mentions with collaged "wisdom."

## Project structure

- `src/markov.{c,h}` — word-level bigram markov chain (hash table with
  chaining; thread-safe via internal mutex). The only module with no Wolfram
  dependency, so its tests run without network access.
- `src/firehose_collector.{c,h}` — single-threaded firehose subscriber that
  parses CAR blocks + DAG-CBOR records and feeds extracted post text into the
  markov model. Uses `wf_subscribe_start` (blocking) with a callback.
- `src/dada_bot.{c,h}` — bot loop: polls `app.bsky.notification.listNotifications`
  for unread mentions, generates a reply from the markov model, and posts it
  via `wf_agent_reply`. Also posts standalone "fortunes" on a timer.
- `src/main.c` — entry point; reads credentials from env, starts the libuv
  event loop with two timers (tick/fortune) and a SIGINT/SIGTERM handler.
  Spawns the firehose collector thread and joins on shutdown.
- `test/test.h` — tiny assert-and-report harness (mirrors Wolfram's).
- `test/test_markov.c` — offline tests for the markov chain.

## Conventions

- **C23 first.** No C++ — the entire project is plain C. The wolfram SDK is
  consumed as a CMake subdirectory and exposes a pure C ABI.
- **Match Wolfram's style.** Follow the conventions in the Wolfram SDK:
  `snake_case` for functions/variables, `WF_OK`/`WF_ERR_*` for status codes when
  calling the SDK, and `/* */` block comments where they aid understanding.
- **Explicit ownership.** Every heap-allocated output from the wolfram SDK is
  freed by its documented `_free` function. The markov model owns its strings.
- **No AI co-authors.** Do not add `Co-authored-by:` trailers crediting AI
  agents to commits.
- **Comments are encouraged** next to public API declarations (ownership
  rules, lifetime, thread-safety) and non-obvious protocol details. Do not add
  noise comments that merely restate the code.

## Threading model

The main thread runs a libuv event loop (`uv_default_loop`). A single side
POSIX thread runs the blocking firehose subscription. Two `uv_timer_t` handles
on the main loop drive the bot logic:

1. **Firehose thread** — `pthread_create` runs `firehose_collector_run`, which
   blocks inside `wf_subscribe_start`. Extracts post text from CAR/CBOR commit
   data and writes to the markov model via `markov_add_text`.

2. **Bot (main loop)** — two libuv timers:
   - `tick_timer` (every 15 s): polls notifications, replies to unread mentions.
   - `fortune_timer` (every `fortune_interval` s): posts standalone fortunes.

The markov model's internal mutex serializes all access between the firehose
writer thread and the bot's timer callbacks. The firehose uses the
subscription's internal curl handle; the bot uses its `wf_agent`'s curl handle,
so no curl handle is shared across threads.

SIGINT and SIGTERM are caught via `uv_signal_t` handles that call `uv_stop`,
draining the timers and the firehose thread cleanly.

## Build & test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The markov chain tests are fully offline. Running `bluesky_dadaist` requires live
Bluesky credentials and a reachable firehose.

## Runtime

```bash
DAFU_HANDLE=oracle.bsky.social DAFU_PASSWORD=... ./build/bluesky_dadaist
```

The bot starts both the firehose collector and the notification-polling loop
immediately. It handles SIGINT/SIGTERM for clean shutdown.

## Debugging

- `DAFU_FIREHOSE` can point at a local PDS WebSocket URL for testing.
- The firehose collector prints a one-line summary per commit if
  `DAFU_VERBOSE=1` is set.
- The markov chain is unit-tested offline; the firehose/bot paths require
  network access and live credentials.
