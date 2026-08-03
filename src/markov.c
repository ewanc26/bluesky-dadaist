/**
 * markov.c — word-level bigram markov chain implementation.
 *
 * See markov.h for the API contract.
 */

#include "markov.h"

#include <stdlib.h>
#include <string.h>

#define DJB2_INIT 5381UL

static unsigned long djb2_hash(const char *s, size_t table_size)
{
    unsigned long h = DJB2_INIT;
    while (*s) {
        h = ((h << 5) + h) + (unsigned char)*s;
        s++;
    }
    return h % table_size;
}

/* ---- internal: find an existing entry for `word` in the hash chain */
static markov_entry *entry_find(markov_model *m, const char *word)
{
    unsigned long h = djb2_hash(word, m->table_size);
    for (markov_entry *e = m->table[h]; e; e = e->next) {
        if (strcmp(e->word, word) == 0)
            return e;
    }
    return NULL;
}

/* ---- internal: create or return the entry for `word` */
static markov_entry *entry_get_or_create(markov_model *m, const char *word)
{
    unsigned long h = djb2_hash(word, m->table_size);

    for (markov_entry *e = m->table[h]; e; e = e->next) {
        if (strcmp(e->word, word) == 0)
            return e;
    }

    markov_entry *e = (markov_entry *)malloc(sizeof(*e));
    if (!e)
        return NULL;

    e->word = strdup(word);
    if (!e->word) {
        free(e);
        return NULL;
    }

    e->followers = NULL;
    e->follower_count = 0;
    e->follower_cap = 0;
    e->next = m->table[h];
    m->table[h] = e;
    m->entry_count++;
    return e;
}

/* ---- internal: append `word` as a follower of `from` */
static void add_transition(markov_model *m, const char *from, const char *to)
{
    markov_entry *e = entry_get_or_create(m, from);
    if (!e)
        return;

    if (e->follower_count >= e->follower_cap) {
        size_t new_cap = e->follower_cap ? e->follower_cap * 2 : 8;
        char **tmp = (char **)realloc(e->followers, new_cap * sizeof(char *));
        if (!tmp)
            return;
        e->followers = tmp;
        e->follower_cap = new_cap;
    }

    char *dup = strdup(to);
    if (!dup)
        return;

    e->followers[e->follower_count++] = dup;
}

int markov_init(markov_model *model, size_t table_size)
{
    if (!model || table_size == 0)
        return 0;

    model->table = (markov_entry **)calloc(table_size, sizeof(markov_entry *));
    if (!model->table)
        return 0;

    model->table_size = table_size;
    model->entry_count = 0;
    pthread_mutex_init(&model->mutex, NULL);
    return 1;
}

void markov_free(markov_model *model)
{
    if (!model || !model->table)
        return;

    pthread_mutex_lock(&model->mutex);

    for (size_t i = 0; i < model->table_size; i++) {
        markov_entry *e = model->table[i];
        while (e) {
            markov_entry *next = e->next;
            free(e->word);
            for (size_t j = 0; j < e->follower_count; j++)
                free(e->followers[j]);
            free(e->followers);
            free(e);
            e = next;
        }
    }

    free(model->table);
    model->table = NULL;
    model->table_size = 0;
    model->entry_count = 0;

    pthread_mutex_unlock(&model->mutex);
    pthread_mutex_destroy(&model->mutex);
}

void markov_add_text(markov_model *model, const char *text)
{
    if (!model || !model->table || !text || !*text)
        return;

    pthread_mutex_lock(&model->mutex);

    /* strtok_r walks `copy` in place; we strdup every token into the model
     * before the next call, so the walk is safe. */
    char *copy = strdup(text);
    if (!copy) {
        pthread_mutex_unlock(&model->mutex);
        return;
    }

    char *saveptr = NULL;
    char *tok = strtok_r(copy, " \t\n\r", &saveptr);

    if (tok) {
        add_transition(model, MARKOV_START, tok);

        char *prev = tok;
        tok = strtok_r(NULL, " \t\n\r", &saveptr);
        while (tok) {
            add_transition(model, prev, tok);
            prev = tok;
            tok = strtok_r(NULL, " \t\n\r", &saveptr);
        }
    }

    free(copy);
    pthread_mutex_unlock(&model->mutex);
}

size_t markov_generate(markov_model *model, size_t max_words,
                       char *out, size_t out_cap)
{
    if (!model || !model->table || !out || out_cap == 0 || max_words == 0)
        return 0;

    pthread_mutex_lock(&model->mutex);

    markov_entry *start = entry_find(model, MARKOV_START);
    if (!start || start->follower_count == 0) {
        pthread_mutex_unlock(&model->mutex);
        out[0] = '\0';
        return 0;
    }

    /* Pick a random first word. */
    const char *current = start->followers[random() % start->follower_count];

    size_t word_count = 0;
    size_t pos = 0;

    while (word_count < max_words) {
        size_t clen = strlen(current);

        /* Need room for: separator + word + NUL. */
        size_t sep = (word_count > 0) ? 1 : 0;
        if (pos + sep + clen + 1 > out_cap)
            break;

        if (sep)
            out[pos++] = ' ';

        memcpy(out + pos, current, clen);
        pos += clen;
        word_count++;

        markov_entry *e = entry_find(model, current);
        if (!e || e->follower_count == 0)
            break;

        current = e->followers[random() % e->follower_count];
    }

    out[pos] = '\0';
    pthread_mutex_unlock(&model->mutex);
    return word_count;
}

size_t markov_entry_count(markov_model *model)
{
    if (!model || !model->table)
        return 0;
    pthread_mutex_lock(&model->mutex);
    size_t n = model->entry_count;
    pthread_mutex_unlock(&model->mutex);
    return n;
}
