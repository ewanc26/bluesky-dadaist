# bluesky-dadaist

> **A Dadaist oracle bot for Bluesky.**  
> It listens to the AT Protocol firehose, builds a word-level markov chain from
> real posts in real-time, and responds to mentions with surreal "oracle" text
> sampled from live social-media chatter. Written in C using the
> **[Wolfram](https://github.com/ewanc26/wolfram) SDK** for the AT Protocol,
> with **libuv** for event-loop dispatch.

## The joke

The Dadaist Oracle is presented as a mystical AI that channels the collective
consciousness of Bluesky. In reality it is a word-level bigram markov chain
fed by whatever people are posting on the firehose *right now*. Every response
is a statistical collage of real posts — absurd, surreal, and never the same
twice.

## How it works

A libuv event loop on the main thread drives two `uv_timer_t` handles while a
side thread runs the blocking firehose subscription:

- **Firehose collector thread** — subscribes to `wss://bsky.network` via
  `wf_subscribe_start`. For each `#commit` event it parses the embedded CAR
  blocks with `wf_car_parse`, locates `app.bsky.feed.post` record blocks by CID
  with `wf_car_find_block`, decodes the DAG-CBOR with `wf_cbor_parse`, and
  extracts the `text` field into the markov model via `markov_add_text`.

- **Oracle bot (main loop)** — logs in via `wf_agent_login`. A 15-second
  `uv_timer_t` polls `app.bsky.notification.listNotifications` for unread
  `@mention`s; for each one it generates a reply with `markov_generate` and
  posts it via `wf_agent_reply`. A second timer posts a standalone "fortune"
  every 30 minutes (configurable) via `wf_agent_post`.

The markov model's internal mutex serializes access between the firehose
writer thread and the bot's reader callbacks. The firehose uses the
subscription's internal curl handle; the bot uses its agent's curl handle, so
no curl handle is shared across threads.

## Requirements

- A C23 compiler (gcc ≥ 14, clang ≥ 18, or Apple clang)
- CMake ≥ 3.20
- libuv ≥ 1.0 (`brew install libuv`)
- The [Wolfram SDK](https://github.com/ewanc26/wolfram) checked out at
  `../wolfram` relative to this repo
- A Bluesky account with an app password

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Usage

```bash
export DAFU_HANDLE=your.bot.bsky.social
export DAFU_PASSWORD=your-app-password
export DAFU_SERVICE=https://bsky.social      # optional, defaults to bsky.social
export DAFU_FIREHOSE=wss://bsky.network      # optional
export DAFU_FORTUNE_INTERVAL=1800             # optional, default 1800s
export DAFU_VERBOSE=1                        # optional, debug output

./build/bluesky_dadaist
```

## Environment variables

| Variable             | Required | Default              | Description                          |
|----------------------|----------|----------------------|--------------------------------------|
| `DAFU_HANDLE`        | yes      | —                    | The bot's Bluesky handle             |
| `DAFU_PASSWORD`      | yes      | —                    | The bot's app password               |
| `DAFU_SERVICE`       | no       | `https://bsky.social` | PDS service URL                      |
| `DAFU_FIREHOSE`      | no       | `wss://bsky.network` | Firehose WebSocket URL               |
| `DAFU_FORTUNE_INTERVAL`| no     | `1800`               | Seconds between standalone fortunes  |
| `DAFU_VERBOSE`       | no       | unset                | Set to `1` for per-event debug output|

## License

AGPL-3.0. See [LICENSE](LICENSE).
