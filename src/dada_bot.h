/**
 * dada_bot.h — the oracle bot.
 *
 * Runs in its own POSIX thread. Logs into a Bluesky account, polls
 * notifications for unread @mentions, and replies with text generated
 * from the shared markov model. Also posts standalone "fortunes"
 * on a configurable interval.
 */

#ifndef DADA_BOT_H
#define DADA_BOT_H

#include "markov.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dada_bot {
    markov_model  *model;          /**< shared model (caller-owned)        */
    char          *handle;         /**< bot handle (e.g. oracle.bsky.social) */
    char          *password;       /**< app password                       */
    char          *service;        /**< PDS URL, e.g. https://bsky.social  */
    volatile int   running;        /**< set to 0 to stop                   */
    int            verbose;        /**< 1 = print per-reply summary        */

    /* Fortune posting interval in seconds. 0 = disabled. */
    int            fortune_interval;
} dada_bot;

/** Thread entry point. Blocks until dada_bot_stop is called. */
void *dada_bot_run(void *arg);

/** Request the bot to stop. Safe to call from any thread. */
void dada_bot_stop(dada_bot *bot);

#ifdef __cplusplus
}
#endif

#endif /* DADA_BOT_H */
