# Contributing to bluesky-dadaist

A Dadaist oracle bot for Bluesky: a word-level Markov chain fed live by the AT Protocol firehose, replying to mentions with surreal "oracle" text sampled from social-media chatter happening right now. C using the [Wolfram](https://github.com/ewanc26/wolfram) SDK, with libuv for event-loop dispatch. Thanks for wanting to help!

The bot is a joke told with a straight face, but it is also a real threaded C program and a nice small demonstration of Wolfram's firehose subscription API.

## Getting started

Building requires a C23 toolchain plus libuv and Wolfram; follow the [README](README.md), including creating `.env` with credentials (app passwords only). Run it locally against a throwaway bot account before any PR.

## Notes for this project

- **Thread discipline is the delicate part.** The firehose collector thread runs the blocking subscription; the oracle bot runs timers on the libuv main loop. The Markov model's mutex serializes access between them, and no curl handle is shared across threads. If your change touches any of that, explain the ownership story in the PR.
- Credential hygiene: tokens go in `.env` (gitignored); never log them, and never post from the maintainer's account in tests.
- Keep the oracle's outputs absurd rather than offensive: the charm is nonsense, and moderation-by-generation-quality is a legitimate PR concern.
- Wolfram's `wf_subscribe_start`, `wf_car_parse`, `wf_cbor_parse`, `wf_agent_reply` paths are the building blocks; don't shell out or reimplement AT Protocol parsing.

## Before opening a PR

- The bot builds and, run against a scratch account, replies coherently (incoherently?) to a test mention.
- Conventional commits; focused PRs; say exactly what you ran.
- AI-assisted PRs welcome; stand behind every line and run it yourself.

## Licence

See [LICENSE](LICENSE). Contributions accepted under the same terms.