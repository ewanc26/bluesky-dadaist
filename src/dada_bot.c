/**
 * dada_bot.c — the oracle bot implementation.
 *
 * Polls app.bsky.notification.listNotifications for unread @mentions.
 * When one is found, generates a reply from the markov model and posts
 * it with wf_agent_reply. Every `fortune_interval` seconds, also posts
 * a standalone fortune to the bot's own feed.
 */

#include "dada_bot.h"

#include <wolfram/agent.h>
#include <wolfram/xrpc.h>

#include <cJSON.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NOTIF_POLL_SECONDS     15
#define MAX_REPLY_WORDS        50
#define MAX_FORTUNE_WORDS      80

/* ---- internal: extract the "text" field from a notification record ---- */
static const char *record_text(const cJSON *record)
{
    if (!record)
        return NULL;
    cJSON *text = cJSON_GetObjectItemCaseSensitive(record, "text");
    if (cJSON_IsString(text))
        return text->valuestring;
    return NULL;
}

/* ---- internal: post a reply to a mentioning post ---- */
static wf_status oracle_reply(wf_agent *agent,
                              const wf_agent_notification *notif,
                              markov_model *model)
{
    char gen[MAX_REPLY_WORDS * 64];  /* generous buffer */
    size_t n = markov_generate(model, MAX_REPLY_WORDS, gen, sizeof(gen));

    if (n == 0) {
        fprintf(stderr, "[bot] model empty — skipping reply\n");
        return WF_OK;
    }

    char reply[512];
    snprintf(reply, sizeof(reply),
             "🔮 The firehose whispers:\n%s", gen);

    wf_agent_post_result out = {0};
    wf_status st = wf_agent_reply(agent, reply,
                                  notif->uri, notif->cid, &out);

    if (st == WF_OK)
        fprintf(stderr, "[bot] replied to %s (uri=%s)\n",
                notif->author.handle ? notif->author.handle : "?",
                out.uri ? out.uri : "(none)");
    else
        fprintf(stderr, "[bot] reply failed: %d\n", (int)st);

    wf_agent_post_result_free(&out);
    return st;
}

/* ---- internal: post a standalone fortune ---- */
static wf_status oracle_fortune(wf_agent *agent, markov_model *model)
{
    char gen[MAX_FORTUNE_WORDS * 64];
    size_t n = markov_generate(model, MAX_FORTUNE_WORDS, gen, sizeof(gen));

    if (n == 0) {
        fprintf(stderr, "[bot] model empty — skipping fortune\n");
        return WF_OK;
    }

    char fortune[512];
    snprintf(fortune, sizeof(fortune),
             "🔮 Dadaist Oracle — firehose reading #%zu:\n%s",
             (size_t)time(NULL), gen);

    wf_agent_post_result out = {0};
    wf_status st = wf_agent_post(agent, fortune, &out);

    if (st == WF_OK)
        fprintf(stderr, "[bot] posted fortune: uri=%s\n",
                out.uri ? out.uri : "(none)");
    else
        fprintf(stderr, "[bot] fortune failed: %d\n", (int)st);

    wf_agent_post_result_free(&out);
    return st;
}

void *dada_bot_run(void *arg)
{
    dada_bot *bot = (dada_bot *)arg;
    if (!bot)
        return NULL;

    const char *service = bot->service ? bot->service : "https://bsky.social";

    wf_agent *agent = wf_agent_new(service);
    if (!agent) {
        fprintf(stderr, "[bot] failed to create agent\n");
        return NULL;
    }

    wf_status st = wf_agent_login(agent, bot->handle, bot->password);
    if (st != WF_OK) {
        fprintf(stderr, "[bot] login failed: %d\n", (int)st);
        wf_agent_free(agent);
        return NULL;
    }

    const char *my_did = wf_agent_get_did(agent);
    fprintf(stderr, "[bot] logged in as %s (%s)\n",
            wf_agent_get_handle(agent),
            my_did ? my_did : "(unknown did)");

    /* Mark the start time for fortune scheduling. */
    time_t last_fortune = 0;

    while (bot->running) {
        /* --- Poll notifications --- */
        wf_agent_notification_list notifs = {0};
        st = wf_agent_list_notifications_typed(agent, 25, NULL, &notifs);
        if (st == WF_OK) {
            char *seen_at = NULL;

            for (size_t i = 0; i < notifs.notification_count; i++) {
                wf_agent_notification *n = &notifs.notifications[i];

                /* Only act on unread mentions, skip our own. */
                if (n->is_read)
                    continue;
                if (n->reason && strcmp(n->reason, "mention") != 0)
                    continue;
                if (n->author.did && my_did &&
                    strcmp(n->author.did, my_did) == 0)
                    continue;
                if (!n->uri || !n->cid)
                    continue;

                /* Remember the latest indexed_at for updateSeen. */
                if (n->indexed_at) {
                    free(seen_at);
                    seen_at = strdup(n->indexed_at);
                }

                if (bot->verbose) {
                    const char *q = record_text(n->record);
                    fprintf(stderr, "[bot] mention from @%s: %.60s\n",
                            n->author.handle ? n->author.handle : "?",
                            q ? q : "(no text)");
                }

                (void)oracle_reply(agent, n, bot->model);
            }

            /* Mark all fetched notifications as seen. */
            if (seen_at) {
                wf_agent_update_seen_notifications(agent, seen_at);
                free(seen_at);
            }
        } else if (st == WF_ERR_HTTP) {
            /* HTTP error — likely rate limited; back off longer. */
            fprintf(stderr, "[bot] notification poll HTTP error: %d\n",
                    (int)st);
        } else {
            fprintf(stderr, "[bot] notification poll failed: %d\n", (int)st);
        }

        wf_agent_notification_list_free(&notifs);

        /* --- Periodic fortune post --- */
        if (bot->fortune_interval > 0) {
            time_t now = time(NULL);
            if (now - last_fortune >= bot->fortune_interval) {
                (void)oracle_fortune(agent, bot->model);
                last_fortune = now;
            }
        }

        /* Sleep in small increments so we notice stop quickly. */
        for (int s = 0; s < NOTIF_POLL_SECONDS && bot->running; s++)
            sleep(1);
    }

    (void)wf_agent_logout(agent);
    wf_agent_free(agent);
    return NULL;
}

void dada_bot_stop(dada_bot *bot)
{
    if (!bot)
        return;
    bot->running = 0;
}
