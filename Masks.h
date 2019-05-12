#ifndef MASKS_H
#define MASKS_H

#include <Common/BasicTypes.h>
#include "OpCodes.h"

uint32 CreateMask(int FirstBit, int LastBit);
uint32 ApplyMask(uint32 val, int FirstBit, int LastBit, bool AllowShift = true, bool ExtendSignBit = false);
EOpCode GetOpCode(int val, uint32 address);

#endif // MASKS_H

