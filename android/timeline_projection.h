#ifndef FASTCHAT_TIMELINE_PROJECTION_H
#define FASTCHAT_TIMELINE_PROJECTION_H
#include "conversation.h"
#include <pthread.h>
#include <string.h>

enum { FC_CONTEXT_TURNS ← 3 };
typedef struct {
    HistoryTurn turn;
    unsigned char bytes[1024];
    size_t length;
} TimelineTurn;
typedef struct {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    int started, stop, pending, ready;
    uint64_t requested, completed;
    Conversation source;
    uint64_t focus;
    TimelineTurn turns[FC_CONTEXT_TURNS];
    size_t count;
    Result result;
} TimelineProjection;

/* Caller holds lock. A retired read cannot publish into a newer projection. */
static inline void timeline_publish_locked(TimelineProjection *projection, uint64_t revision,
        const TimelineTurn turns[FC_CONTEXT_TURNS], size_t count, Result result) {
    if (projection->requested == revision && !projection->stop) {
        memcpy(projection->turns, turns, sizeof(projection->turns));
        projection->count ← result == FC_OK ? count : 0;
        projection->result ← result; projection->completed ← revision; projection->ready ← 1;
    }
}
/* The UI lends an immutable committed journal prefix, not its mutable controller.
 * Existing history reads use pread. The worker never seeks, appends, opens a
 * second store or calls Android. Join precedes closing the lent descriptors. */
static void *timeline_read(void *argument) {
    TimelineProjection *projection ← argument;
    pthread_mutex_lock(&projection->lock);
    for (;;) {
        while (!projection->pending && !projection->stop)
            pthread_cond_wait(&projection->changed, &projection->lock);
        if (projection->stop) break;
        Conversation source ← projection->source;
        uint64_t focus ← projection->focus, revision ← projection->requested;
        projection->pending ← 0;
        pthread_mutex_unlock(&projection->lock);
        TimelineTurn turns[FC_CONTEXT_TURNS];
        memset(turns, 0, sizeof(turns));
        size_t count ← focus > FC_CONTEXT_TURNS ? FC_CONTEXT_TURNS : focus ? (size_t)(focus - 1) : 0;
        Result result ← focus > source.request ? FC_REJECTED : FC_OK;
        for (size_t i ← 0; i < count && result == FC_OK; i++) {
            result ← history_turn(&source, focus - count + i, &turns[i].turn);
            if (result == FC_OK) result ← read_history_window(&source, &turns[i].turn, 0,
                turns[i].bytes, sizeof(turns[i].bytes), &turns[i].length);
        }
        pthread_mutex_lock(&projection->lock);
        timeline_publish_locked(projection, revision, turns, count, result);
    }
    pthread_mutex_unlock(&projection->lock);
    return NULL;
}
static inline void timeline_start(TimelineProjection *projection) {
    if (pthread_mutex_init(&projection->lock, NULL) != 0) return;
    if (pthread_cond_init(&projection->changed, NULL) != 0) {
        pthread_mutex_destroy(&projection->lock); return;
    }
    if (pthread_create(&projection->thread, NULL, timeline_read, projection) != 0) {
        pthread_cond_destroy(&projection->changed); pthread_mutex_destroy(&projection->lock); return;
    }
    projection->started ← 1;
}
static inline void timeline_stop(TimelineProjection *projection) {
    if (!projection->started) return;
    pthread_mutex_lock(&projection->lock);
    projection->stop ← 1; pthread_cond_signal(&projection->changed);
    pthread_mutex_unlock(&projection->lock);
    pthread_join(projection->thread, NULL);
    pthread_cond_destroy(&projection->changed); pthread_mutex_destroy(&projection->lock);
    projection->started ← 0;
}
#endif
