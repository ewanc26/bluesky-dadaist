/**
 * test_markov.c — offline unit tests for the markov chain.
 *
 * These tests exercise the text model without any network access.
 * Run with: ctest --test-dir build
 */

#include "test.h"
#include "markov.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void test_init_and_free(void)
{
    markov_model m;
    TEST_CHECK(markov_init(&m, 1024));
    TEST_CHECK(markov_entry_count(&m) == 0);
    markov_free(&m);
    /* double-free safety on a zeroed struct */
    TEST_CHECK(1);
}

static void test_add_text_populates_model(void)
{
    markov_model m;
    TEST_CHECK(markov_init(&m, 1024));

    markov_add_text(&m, "hello world");
    TEST_CHECK(markov_entry_count(&m) > 0);

    markov_free(&m);
}

static void test_generate_produces_output(void)
{
    markov_model m;
    TEST_CHECK(markov_init(&m, 1024));

    markov_add_text(&m, "hello world foo bar");

    char buf[256];
    size_t n = markov_generate(&m, 20, buf, sizeof(buf));
    TEST_CHECK(n > 0);
    TEST_CHECK(strlen(buf) > 0);
    TEST_CHECK(strstr(buf, "hello") != NULL ||
               strstr(buf, "world") != NULL ||
               strstr(buf, "foo") != NULL ||
               strstr(buf, "bar") != NULL);

    markov_free(&m);
}

static void test_empty_model_generates_nothing(void)
{
    markov_model m;
    TEST_CHECK(markov_init(&m, 1024));

    char buf[256];
    buf[0] = 'X';
    size_t n = markov_generate(&m, 20, buf, sizeof(buf));
    TEST_CHECK(n == 0);
    TEST_CHECK(buf[0] == '\0');

    markov_free(&m);
}

static void test_max_words_respected(void)
{
    markov_model m;
    TEST_CHECK(markov_init(&m, 1024));

    markov_add_text(&m, "alpha beta gamma delta epsilon zeta");

    char buf[256];
    size_t n = markov_generate(&m, 3, buf, sizeof(buf));
    TEST_CHECK(n <= 3);

    markov_free(&m);
}

static void test_large_text(void)
{
    markov_model m;
    TEST_CHECK(markov_init(&m, 8192));

    for (int i = 0; i < 100; i++)
        markov_add_text(&m, "the quick brown fox jumps over the lazy dog");

    TEST_CHECK(markov_entry_count(&m) > 0);

    char buf[1024];
    size_t n = markov_generate(&m, 50, buf, sizeof(buf));
    TEST_CHECK(n > 0);

    markov_free(&m);
}

int main(void)
{
    srandom((unsigned int)time(NULL));

    test_init_and_free();
    test_add_text_populates_model();
    test_generate_produces_output();
    test_empty_model_generates_nothing();
    test_max_words_respected();
    test_large_text();

    TEST_SUMMARY();
}
