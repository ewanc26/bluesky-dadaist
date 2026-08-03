/**
 * firehose_collector.h — listen to the AT Protocol firehose and feed
 * every post's text into a markov model.
 *
 * Designed to run in its own POSIX thread. The collector blocks inside
 * wf_subscribe_start until firehose_collector_stop is called from another
 * thread — the event callback checks the `running` flag and issues
 * wf_subscribe_stop when it goes low.
 */

#ifndef DADA_FIREHOSE_COLLECTOR_H
#define DADA_FIREHOSE_COLLECTOR_H

#include "markov.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct firehose_collector {
    markov_model  *model;          /**< shared model (caller-owned)        */
    const char    *service;        /**< e.g. "wss://bsky.network"          */
    volatile int   running;        /**< set to 0 by firehose_collector_stop */
    int            verbose;        /**< 1 = print per-commit summary        */
} firehose_collector;

/** Thread entry point. Blocks until stop is requested via the callback. */
void *firehose_collector_run(void *arg);

/** Request the collector to stop. Safe to call from any thread. The call
 * returns immediately; the subscription loop drains and exits on its own. */
void firehose_collector_stop(firehose_collector *fc);

#ifdef __cplusplus
}
#endif

#endif /* DADA_FIREHOSE_COLLECTOR_H */
