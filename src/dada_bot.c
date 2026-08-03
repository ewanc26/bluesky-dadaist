/**
 * dada_bot.c — the oracle bot, driven by libuv timers.
 *
 * dada_bot_on_tick      — fires every NOTIF_POLL_SECONDS: polls
 *                         app.bsky.notification.listNotifications for unread
 *                         @mentions and replies with markov-generated text.
 *
 * dada_bot_on_fortune   — fires every fortune_interval seconds: posts a
 *                         standalone "Dadaist oracle" fortune.
 */

#include "dada_bot.h"

#include <wolfram/agent.h>

#include <cJSON.h>

#include <uv.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_REPLY_WORDS    50
#define MAX_FORTUNE_WORDS  80

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

/* ---- internal: reply to a mentioning post ---- */
static void oracle_reply(wf_agent *agent,
                         const wf_agent_notification *notif,
                         markov_model *model, int verbose)
{
    char gen[MAX_REPLY_WORDS * 64];
    size_t n = markov_generate(model, MAX_REPLY_WORDS, gen, sizeof(gen));

    if (n == 0) {
        if (verbose)
            fprintf(stderr, "[bot] model empty — skipping reply\n");
        return;
    }

    char reply[512];
    snprintf(reply, sizeof(reply),
             "🔮 The firehose whispers:\n%s", gen);

    wf_agent_post_result out = {0};
    wf_status st = wf_agent_reply(agent, reply,
                                  notif->uri, notif->cid, &out);

    if (st == WF_OK) {
        if (verbose)
            fprintf(stderr, "[bot] replied to @%s\n",
                    notif->author.handle ? notif->author.handle : "?");
    } else {
        fprintf(stderr, "[bot] reply failed: %d\n", (int)st);
    }

    wf_agent_post_result_free(&out);
}

/* ---- internal: post a standalone fortune ---- */
static void oracle_fortune(wf_agent *agent, markov_model *model, int verbose)
{
    char gen[MAX_FORTUNE_WORDS * 64];
    size_t n = markov_generate(model, MAX_FORTUNE_WORDS, gen, sizeof(gen));

    if (n == 0) {
        if (verbose)
            fprintf(stderr, "[bot] model empty — skipping fortune\n");
        return;
    }

    char fortune[512];
    snprintf(fortune, sizeof(fortune),
             "🔮 Dadaist Oracle — firehose reading #%d:\n%s",
             (int)time(NULL), gen);

    wf_agent_post_result out = {0};
    wf_status st = wf_agent_post(agent, fortune, &out);

    if (st == WF_OK) {
        if (verbose)
            fprintf(stderr, "[bot] posted fortune: %s\n",
                    out.uri ? out.uri : "(posted)");
    } else {
        fprintf(stderr, "[bot] fortune failed: %d\n", (int)st);
    }

    wf_agent_post_result_free(&out);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

wf_status dada_bot_login(dada_bot *bot)
{
    if (!bot)
        return WF_ERR_INVALID_ARG;

    const char *service = bot->service ? bot->service : "https://bsky.social";

    bot->agent = wf_agent_new(service);
    if (!bot->agent) {
        fprintf(stderr, "[bot] failed to create agent\n");
        return WF_ERR_ALLOC;
    }

    wf_status st = wf_agent_login(bot->agent, bot->handle, bot->password);
    if (st != WF_OK) {
        fprintf(stderr, "[bot] login failed: %d\n", (int)st);
        wf_agent_free(bot->agent);
        bot->agent = NULL;
        return st;
    }

    bot->my_did = wf_agent_get_did(bot->agent);

    fprintf(stderr, "[bot] logged in as %s (%s)\n",
            wf_agent_get_handle(bot->agent),
            bot->my_did ? bot->my_did : "(unknown did)");
    return WF_OK;
}

void dada_bot_logout(dada_bot *bot)
{
    if (!bot || !bot->agent)
        return;
    (void)wf_agent_logout(bot->agent);
    wf_agent_free(bot->agent);
    bot->agent = NULL;
    bot->my_did = NULL;
}

void dada_bot_on_tick(uv_timer_t *timer)
{
    dada_bot *bot = (dada_bot *)timer->data;
    if (!bot || !bot->agent)
        return;

    wf_agent_notification_list notifs = {0};
    wf_status st = wf_agent_list_notifications_typed(
        bot->agent, 25, NULL, &notifs);

    if (st == WF_OK) {
        char *seen_at = NULL;

        for (size_t i = 0; i < notifs.notification_count; i++) {
            wf_agent_notification *n = &notifs.notifications[i];

            if (n->is_read)
                continue;
            if (n->reason && strcmp(n->reason, "mention") != 0)
                continue;
            if (n->author.did && bot->my_did &&
                strcmp(n->author.did, bot->my_did) == 0)
                continue;
            if (!n->uri || !n->cid)
                continue;

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

            oracle_reply(bot->agent, n, bot->model, bot->verbose);
        }

        if (seen_at) {
            wf_agent_update_seen_notifications(bot->agent, seen_at);
            free(seen_at);
        }
    } else if (st == WF_ERR_HTTP) {
        fprintf(stderr, "[bot] notification poll HTTP error: %d\n", (int)st);
    } else {
        fprintf(stderr, "[bot] notification poll failed: %d\n", (int)st);
    }

    wf_agent_notification_list_free(&notifs);
}

void dada_bot_on_fortune(uv_timer_t *timer)
{
    dada_bot *bot = (dada_bot *)timer->data;
    if (!bot || !bot->agent)
        return;

    oracle_fortune(bot->agent, bot->model, bot->verbose);
}
