// boards/colt_uaefi/colt_can.h
#pragma once

#include <stdint.h>

void initColtCan();
void processColtCanTx();
void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc);