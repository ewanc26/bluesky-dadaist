/**
 * main.c — bsky-dada entry point.
 *
 * Reads credentials from environment variables, spawns the firehose
 * collector and oracle bot threads, installs signal handlers for clean
 * shutdown, and joins on exit.
 */

#include "firehose_collector.h"
#include "dada_bot.h"
#include "markov.h"

#include <wolfram/xrpc.h>

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_FIREHOSE "wss://bsky.network"
#define DEFAULT_SERVICE  "https://bsky.social"
#define DEFAULT_TABLE    65536

static volatile sig_atomic_t g_terminated = 0;

static void handle_signal(int sig)
{
    (void)sig;
    g_terminated = 1;
}

static const char *env(const char *key, const char *fallback)
{
    const char *v = getenv(key);
    return v ? v : fallback;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const char *handle   = getenv("DAFU_HANDLE");
    const char *password = getenv("DAFU_PASSWORD");

    if (!handle || !password) {
        fprintf(stderr,
                "usage: set DAFU_HANDLE, DAFU_PASSWORD\n"
                "optional: DAFU_SERVICE (default %s)\n"
                "          DAFU_FIREHOSE (default %s)\n"
                "          DAFU_FORTUNE_INTERVAL (default 1800)\n"
                "          DAFU_VERBOSE=1 for debug output\n",
                DEFAULT_SERVICE, DEFAULT_FIREHOSE);
        return 1;
    }

    const char *service     = env("DAFU_SERVICE", DEFAULT_SERVICE);
    const char *firehose    = env("DAFU_FIREHOSE", DEFAULT_FIREHOSE);
    int verbose             = getenv("DAFU_VERBOSE") ? 1 : 0;
    int fortune_interval    = 1800; /* 30 minutes */
    const char *fi_env       = getenv("DAFU_FORTUNE_INTERVAL");
    if (fi_env)
        fortune_interval = atoi(fi_env);

    fprintf(stderr, "[main] bsky-dada — Dadaist Oracle bot\n");
    fprintf(stderr, "[main] firehose: %s\n", firehose);
    fprintf(stderr, "[main] service:  %s\n", service);
    fprintf(stderr, "[main] handle:   %s\n", handle);

    /* ---- markov model ---- */
    markov_model model;
    if (!markov_init(&model, DEFAULT_TABLE)) {
        fprintf(stderr, "[main] failed to initialise markov model\n");
        return 1;
    }
    srandom((unsigned int)time(NULL));

    /* ---- firehose collector ---- */
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

    /* ---- oracle bot ---- */
    dada_bot bot = {0};
    bot.model            = &model;
    bot.handle           = strdup(handle);
    bot.password         = strdup(password);
    bot.service          = strdup(service);
    bot.verbose          = verbose;
    bot.fortune_interval = fortune_interval;
    bot.running           = 1;

    pthread_t bot_tid;
    if (pthread_create(&bot_tid, NULL, dada_bot_run, &bot) != 0) {
        fprintf(stderr, "[main] failed to create bot thread\n");
        firehose_collector_stop(&fc);
        pthread_join(firehose_tid, NULL);
        free(bot.handle);
        free(bot.password);
        free(bot.service);
        markov_free(&model);
        return 1;
    }

    /* ---- signal handler for graceful shutdown ---- */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    fprintf(stderr, "[main] running — press Ctrl-C to stop\n");

    while (!g_terminated)
        sleep(1);

    fprintf(stderr, "[main] shutting down...\n");
    firehose_collector_stop(&fc);
    dada_bot_stop(&bot);

    pthread_join(firehose_tid, NULL);
    pthread_join(bot_tid, NULL);

    free(bot.handle);
    free(bot.password);
    free(bot.service);
    markov_free(&model);

    fprintf(stderr, "[main] bye\n");
    return 0;
}
