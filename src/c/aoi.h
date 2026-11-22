#ifndef LUAAOI_SPACE_H
#define LUAAOI_SPACE_H

#include <stdint.h>

struct aoi_space;
typedef void aoi_Callback(void *ud, uint32_t watcher, uint32_t marker);

struct aoi_space *aoi_new(void);
void aoi_release(struct aoi_space *space);
/* Return zero on allocation failure. Retry message with an empty collector. */
int aoi_update(struct aoi_space *space, uint32_t id, const char *mode, const float pos[3]);
int aoi_message(struct aoi_space *space, aoi_Callback *collect, void *ud);

#endif
