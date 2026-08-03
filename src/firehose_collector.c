/**
 * firehose_collector.c — subscribe to the AT Protocol firehose and extract
 * post text from commit events.
 *
 * For each WF_SUBSCRIBE_EVENT_COMMIT the collector:
 *   1. Parses the embedded CAR blocks with wf_car_parse.
 *   2. Iterates the commit's ops; for every "create" on
 *      app.bsky.feed.post it locates the record block by CID
 *      (wf_car_find_block).
 *   3. Decodes the block as DAG-CBOR (wf_cbor_parse) and extracts the
 *      "text" field into a temporary NUL-terminated buffer.
 *   4. Feeds the text into the markov model via markov_add_text.
 */

#include "firehose_collector.h"

#include <wolfram/repo/car.h>
#include <wolfram/repo/cbor.h>
#include <wolfram/sync_subscribe.h>
#include <wolfram/xrpc.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum number of posts we'll harvest from a single commit. */
#define MAX_POSTS_PER_COMMIT 64

/* Shared handle pointer so the stop flag can reach the callback. */
static wf_subscribe_handle *g_subscribe_handle;
static pthread_mutex_t g_handle_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---- internal: pull the "text" field out of a DAG-CBOR record map ---- */
static void extract_post_text(const wf_cbor_item *item,
                              markov_model *model)
{
    if (!item || item->type != WF_CBOR_MAP)
        return;

    for (size_t i = 0; i < item->map.count; i++) {
        wf_cbor_pair *pair = &item->map.pairs[i];
        if (!pair->key || pair->key->type != WF_CBOR_STRING)
            continue;
        if (!pair->value || pair->value->type != WF_CBOR_STRING)
            continue;

        /* Match "text" by length-prefixed compare. */
        static const char text_key[] = "text";
        if (pair->key->string.len == sizeof(text_key) - 1 &&
            memcmp(pair->key->string.str, text_key,
                   sizeof(text_key) - 1) == 0) {
            const char *src = pair->value->string.str;
            size_t len = pair->value->string.len;

            char *buf = (char *)malloc(len + 1);
            if (!buf)
                return;
            memcpy(buf, src, len);
            buf[len] = '\0';

            markov_add_text(model, buf);
            free(buf);
            return;  /* first "text" field wins */
        }
    }
}

/* ---- internal: harvest every post record from a commit event ---- */
static int collect_from_commit(const wf_subscribe_commit *commit,
                               firehose_collector *fc)
{
    if (!commit->blocks || commit->blocks_len == 0)
        return 0;

    wf_car car;
    memset(&car, 0, sizeof(car));
    if (wf_car_parse(commit->blocks, commit->blocks_len, &car) != WF_OK)
        return 0;

    int collected = 0;

    for (size_t i = 0; i < commit->ops_count && collected < MAX_POSTS_PER_COMMIT;
         i++) {
        wf_subscribe_repo_op *op = &commit->ops[i];

        if (op->action[0] == '\0' ||
            strcmp(op->action, "create") != 0)
            continue;

        if (!op->has_cid || !op->path)
            continue;

        /* Match collection prefix "app.bsky.feed.post/". */
        static const char post_prefix[] = "app.bsky.feed.post/";
        if (strncmp(op->path, post_prefix,
                    sizeof(post_prefix) - 1) != 0)
            continue;

        wf_car_block *block = wf_car_find_block(&car, &op->cid);
        if (!block || !block->data || block->data_len == 0)
            continue;

        wf_cbor_item *root = wf_cbor_parse(block->data, block->data_len);
        if (!root)
            continue;

        if (root->type == WF_CBOR_MAP) {
            extract_post_text(root, fc->model);
            collected++;
        }
        wf_cbor_free(root);
    }

    wf_car_free(&car);
    return collected;
}

/* ---- subscribe callbacks ---- */

static void on_event(const wf_subscribe_event *event, void *userdata)
{
    firehose_collector *fc = (firehose_collector *)userdata;
    if (!fc || !event)
        return;

    if (event->type == WF_SUBSCRIBE_EVENT_COMMIT) {
        int n = collect_from_commit(&event->data.commit, fc);
        if (fc->verbose && n > 0) {
            fprintf(stderr, "[firehose] harvested %d post(s) "
                    "(did=%.16s… seq=%lld)\n",
                    n, event->data.commit.did,
                    (long long)event->data.commit.seq);
        }
    }

    /* Request shutdown if another thread has asked us to stop. */
    if (!fc->running) {
        pthread_mutex_lock(&g_handle_lock);
        if (g_subscribe_handle)
            wf_subscribe_stop(g_subscribe_handle);
        pthread_mutex_unlock(&g_handle_lock);
    }
}

static void on_error(wf_status status, const char *msg, void *userdata)
{
    firehose_collector *fc = (firehose_collector *)userdata;
    (void)fc;
    fprintf(stderr, "[firehose] error: status=%d %s\n", (int)status,
            msg ? msg : "(no message)");
}

void *firehose_collector_run(void *arg)
{
    firehose_collector *fc = (firehose_collector *)arg;
    if (!fc || !fc->model)
        return NULL;

    srandom((unsigned int)time(NULL));

    wf_subscribe_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.service = fc->service ? fc->service : "wss://bsky.network";
    opts.cursor = 0;
    opts.has_cursor = 0;
    opts.on_event = on_event;
    opts.on_error = on_error;
    opts.userdata = fc;
    opts.max_retry_seconds = 30;
    opts.reconnect_delay_ms = 2000;
    opts.ping_interval_ms = 30000;

    g_subscribe_handle = NULL;

    wf_status st = wf_subscribe_start(&opts, &g_subscribe_handle);

    if (st == WF_OK)
        fprintf(stderr, "[firehose] subscription closed cleanly\n");
    else
        fprintf(stderr, "[firehose] subscription ended with status %d\n",
                (int)st);

    return NULL;
}

void firehose_collector_stop(firehose_collector *fc)
{
    if (!fc)
        return;
    fc->running = 0;
}
