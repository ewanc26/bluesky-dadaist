/**
 * main.c — bluesky-dadaist entry point.
 *
 * Initialises the markov model and bot, then runs a libuv event loop that
 * drives a periodic notification-polling timer and a fortune-posting timer.
 * The firehose collector runs in a side thread.
 */

#include "dada_bot.h"
#include "firehose_collector.h"
#include "markov.h"

#include <uv.h>

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_FIREHOSE "wss://bsky.network"
#define DEFAULT_SERVICE  "https://bsky.social"
#define DEFAULT_TABLE    65536
#define DEFAULT_FORTUNE  1800

static const char *env_or(const char *key, const char *fallback)
{
    const char *v = getenv(key);
    return v ? v : fallback;
}

static void on_signal(uv_signal_t *w, int sig)
{
    (void)sig;
    fprintf(stderr, "[main] signal received, shutting down...\n");
    uv_stop(uv_handle_get_loop((uv_handle_t *)w));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *handle   = getenv("DAFU_HANDLE");
    const char *password = getenv("DAFU_PASSWORD");

    if (!handle || !password) {
        fprintf(stderr,
                "usage: set DAFU_HANDLE and DAFU_PASSWORD\n"
                "optional: DAFU_SERVICE (default %s)\n"
                "          DAFU_FIREHOSE (default %s)\n"
                "          DAFU_FORTUNE_INTERVAL (default %d)\n"
                "          DAFU_VERBOSE=1 for debug output\n",
                DEFAULT_SERVICE, DEFAULT_FIREHOSE, DEFAULT_FORTUNE);
        return 1;
    }

    const char *service     = env_or("DAFU_SERVICE", DEFAULT_SERVICE);
    const char *firehose    = env_or("DAFU_FIREHOSE", DEFAULT_FIREHOSE);
    int verbose             = getenv("DAFU_VERBOSE") ? 1 : 0;
    int fortune_interval    = DEFAULT_FORTUNE;
    const char *fi_env       = getenv("DAFU_FORTUNE_INTERVAL");
    if (fi_env)
        fortune_interval = atoi(fi_env);

    fprintf(stderr, "[main] bluesky-dadaist — Dadaist Oracle bot\n");
    fprintf(stderr, "[main] firehose: %s\n", firehose);
    fprintf(stderr, "[main] service:  %s\n", service);
    fprintf(stderr, "[main] handle:   %s\n", handle);

    /* ---- markov model ---- */
    markov_model model;
    if (!markov_init(&model, DEFAULT_TABLE)) {
        fprintf(stderr, "[main] failed to initialise markov model\n");
        return 1;
    }
    srandom((unsigned)time(NULL));

    /* ---- firehose collector (side thread) ---- */
    firehose_collector fc = {0};
    fc.model   = &model;
    fc.service = firehose;
    fc.verbose = verbose;
    fc.running = 1;

    pthread_t firehose_tid;
    if (pthread_create(&firehose_tid, NULL, firehose_collector_run, &fc) != 0) {
        fprintf(stderr, "[main] failed to create firehose thread\n");
        markov_free(&model);
        return 1;
    }

    /* ---- bot login ---- */
    dada_bot bot = {0};
    bot.model            = &model;
    bot.handle           = strdup(handle);
    bot.password         = strdup(password);
    bot.service          = strdup(service);
    bot.verbose          = verbose;
    bot.fortune_interval = fortune_interval;

    wf_status st = dada_bot_login(&bot);
    if (st != WF_OK) {
        fprintf(stderr, "[main] bot login failed\n");
        firehose_collector_stop(&fc);
        pthread_join(firehose_tid, NULL);
        free(bot.handle); free(bot.password); free(bot.service);
        markov_free(&model);
        return 1;
    }

    /* ---- libuv event loop ---- */
    uv_loop_t *loop = uv_default_loop();

    uv_timer_t tick_timer;
    tick_timer.data = &bot;
    uv_timer_init(loop, &tick_timer);
    uv_timer_start(&tick_timer, dada_bot_on_tick, 0,
                   NOTIF_POLL_SECONDS * 1000);

    uv_timer_t fortune_timer;
    fortune_timer.data = &bot;
    uv_timer_init(loop, &fortune_timer);
    if (fortune_interval > 0) {
        uv_timer_start(&fortune_timer, dada_bot_on_fortune,
                       fortune_interval * 1000,
                       fortune_interval * 1000);
    }

    uv_signal_t sigint_watcher;
    uv_signal_t sigterm_watcher;
    uv_signal_init(loop, &sigint_watcher);
    uv_signal_init(loop, &sigterm_watcher);
    uv_signal_start(&sigint_watcher, on_signal, SIGINT);
    uv_signal_start(&sigterm_watcher, on_signal, SIGTERM);

    fprintf(stderr, "[main] running — Ctrl-C to stop\n");
    uv_run(loop, UV_RUN_DEFAULT);

    /* ---- shutdown ---- */
    fprintf(stderr, "[main] stopping firehose collector\n");
    firehose_collector_stop(&fc);
    pthread_join(firehose_tid, NULL);

    fprintf(stderr, "[main] logging out bot\n");
    dada_bot_logout(&bot);

    uv_timer_stop(&tick_timer);
    uv_timer_stop(&fortune_timer);
    uv_close((uv_handle_t *)&tick_timer, NULL);
    uv_close((uv_handle_t *)&fortune_timer, NULL);
    uv_close((uv_handle_t *)&sigint_watcher, NULL);
    uv_close((uv_handle_t *)&sigterm_watcher, NULL);
    uv_run(loop, UV_RUN_DEFAULT);

    free(bot.handle);
    free(bot.password);
    free(bot.service);
    markov_free(&model);

    uv_loop_close(loop);

    fprintf(stderr, "[main] bye\n");
    return 0;
}
