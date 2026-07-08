#ifndef FDCAN_H
#define FDCAN_H

#include <stdint.h>

//prototypes
void fdcan_init(void);
void fdcan_send(uint32_t id, uint32_t d0, uint32_t d1);
void fdcan_recv(uint32_t *id, uint32_t *dlc, uint32_t *d0, uint32_t *d1);

#endif