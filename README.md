# bsky-dada

> **A Dadaist oracle bot for Bluesky.**  
> It listens to the AT Protocol firehose, builds a word-level markov chain from
> real posts in real-time, and responds to mentions with surreal "oracle" text
> sampled from live social-media chatter. Written in C using the
> **[Wolfram](https://github.com/ewanc26/wolfram) SDK** for the AT Protocol.

## The joke

The Dadaist Oracle is presented as a mystical AI that channels the collective
consciousness of Bluesky. In reality it is a word-level bigram markov chain
fed by whatever people are posting on the firehose *right now*. Every response
is a statistical collage of real posts — absurd, surreal, and never the same
twice.

## How it works

Two POSIX threads share a single markov model, each built on a different part
of the Wolfram SDK:

- **Firehose collector thread** — subscribes to `wss://bsky.network` via
  `wf_subscribe_start`. For each `#commit` event it parses the embedded CAR
  blocks with `wf_car_parse`, locates `app.bsky.feed.post` record blocks by CID
  with `wf_car_find_block`, decodes the DAG-CBOR with `wf_cbor_parse`, and
  extracts the `text` field into the markov model via `markov_add_text`.

- **Oracle bot thread** — logs in via `wf_agent_login`, then polls
  `app.bsky.notification.listNotifications` every 15 seconds. For each unread
  `@mention` it generates a reply with `markov_generate` and posts it via
  `wf_agent_reply`. Every 30 minutes it also posts a standalone "fortune" to
  its own feed.

## Requirements

- A C23 compiler (gcc ≥ 14, clang ≥ 18, or Apple clang)
- CMake ≥ 3.20
- libcurl, OpenSSL (provided by the Wolfram SDK build)
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

./build/bsky_dada
```

## Environment variables

| Variable          | Required | Default             | Description                         |
|-------------------|----------|---------------------|-------------------------------------|
| `DAFU_HANDLE`     | yes      | —                   | The bot's Bluesky handle            |
| `DAFU_PASSWORD`   | yes      | —                   | The bot's app password              |
| `DAFU_SERVICE`    | no       | `https://bsky.social` | PDS service URL                    |
| `DAFU_FIREHOSE`   | no       | `wss://bsky.network` | Firehose WebSocket URL              |

## License

AGPL-3.0. See [LICENSE](LICENSE).
