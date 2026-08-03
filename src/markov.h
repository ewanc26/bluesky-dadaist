/**
 * markov.h — word-level bigram markov chain.
 *
 * Thread-safe: all public functions acquire an internal mutex. The firehose
 * collector thread calls markov_add_text (writer); the bot thread calls
 * markov_generate (reader). The mutex serializes both.
 *
 * Ownership: all strings stored in the model are heap-allocated copies.
 * markov_free releases every allocation.
 */

#ifndef DADA_MARKOV_H
#define DADA_MARKOV_H

#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Internal: a hash-table bucket entry. */
typedef struct markov_entry {
    char  *word;            /**< heap-allocated key string            */
    char **followers;       /**< dynamic array of heap-allocated followers */
    size_t follower_count;  /**< number of followers currently stored      */
    size_t follower_cap;    /**< capacity of the followers array            */
    struct markov_entry *next; /**< hash-chain linkage                      */
} markov_entry;

typedef struct markov_model {
    markov_entry **table;
    size_t table_size;
    size_t entry_count;
    pthread_mutex_t mutex;
} markov_model;

/* The sentinel token that seeds every text. When generating, the chain
 * starts here and follows a random first-word edge. */
#define MARKOV_START "\x02START\x02"

/**
 * Initialise a model with a hash table of `table_size` buckets.
 * Returns 1 on success, 0 on allocation failure.
 */
int markov_init(markov_model *model, size_t table_size);

/**
 * Release all memory held by the model (entries, strings, arrays,
 * mutex). Safe to call on a zeroed struct.
 */
void markov_free(markov_model *model);

/**
 * Tokenise `text` on whitespace and insert every adjacent word pair
 * into the chain. Prepends MARKOV_START so generation can begin from
 * a real first-word edge. The start sentinel itself is never emitted.
 */
void markov_add_text(markov_model *model, const char *text);

/**
 * Walk the chain and fill `out` with at most `max_words` generated words,
 * space-separated and NUL-terminated. Returns the word count written
 * (0 on an empty model or write failure).
 */
size_t markov_generate(markov_model *model, size_t max_words,
                       char *out, size_t out_cap);

/** Number of unique words currently in the model. */
size_t markov_entry_count(markov_model *model);

#ifdef __cplusplus
}
#endif

#endif /* DADA_MARKOV_H */
