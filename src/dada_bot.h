/**
 * dada_bot.h — the oracle bot.
 *
 * Designed to be driven from a libuv event loop. The caller logs the bot
 * in (dada_bot_login), then arms two uv_timer_t handles whose callbacks
 * (dada_bot_on_tick, dada_bot_on_fortune) poll notifications and post
 * fortunes respectively. dada_bot_logout tears down the session.
 */

#ifndef DADA_BOT_H
#define DADA_BOT_H

#include "markov.h"

#include <wolfram/agent.h>
#include <uv.h>

#define NOTIF_POLL_SECONDS 15

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dada_bot {
    markov_model  *model;
    char          *handle;
    char          *password;
    char          *service;
    int            verbose;

    /* Fortune interval in seconds. 0 = disabled. */
    int            fortune_interval;

    /* Runtime state (populated by dada_bot_login). */
    wf_agent      *agent;    /* owned; NULL until login       */
    const char    *my_did;   /* borrowed; valid while logged in */
} dada_bot;

/**
 * Log in the bot. Allocates the internal wf_agent.
 * Returns WF_OK on success.
 */
wf_status dada_bot_login(dada_bot *bot);

/**
 * Log out and free the internal agent. Safe to call after login.
 */
void dada_bot_logout(dada_bot *bot);

/**
 * libuv timer callback: poll notifications, reply to unread mentions.
 * Set as the callback on a uv_timer_t whose `data` points to the dada_bot.
 */
void dada_bot_on_tick(uv_timer_t *timer);

/**
 * libuv timer callback: post a standalone fortune.
 * Set as the callback on a uv_timer_t whose `data` points to the dada_bot.
 */
void dada_bot_on_fortune(uv_timer_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* DADA_BOT_H */
