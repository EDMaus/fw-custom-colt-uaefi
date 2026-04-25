// boards/uaefi_pro/colt_can.h
#pragma once

#include <stdint.h>

class CanCycle;

void initColtCan();
void processColtCanTx(CanCycle cycle);
void processColtCanRx(uint32_t id, const uint8_t* data, uint8_t dlc);
