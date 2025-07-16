#ifndef AUDIO_FEATURE_STATS_H
#define AUDIO_FEATURE_STATS_H

typedef int (*afsCallback)(void **, size_t *);

typedef int (*afs_init_t)(afsCallback);
typedef int (*afs_deinit_t)(void);

#endif /* AUDIO_FEATURE_STATS_H */
