#include "Masks.h"
#include "OpCodes.h"
#include <Common/TString.h>
#include <iostream>

uint32 CreateMask(int FirstBit, int LastBit)
{
    uint32 out = 0;

    for (int i = FirstBit; i <= LastBit; i++)
        out |= 0x80000000 >> i;

    return out;
}

uint32 ApplyMask(uint32 val, int FirstBit, int LastBit, bool AllowShift /*= true*/, bool ExtendSignBit /*= true*/)
{
    uint32 mask = CreateMask(FirstBit, LastBit);
    val &= mask;

    if (mask && AllowShift)
    {
        while (!(mask & 0x1))
        {
            mask >>= 1;
            val >>= 1;
            FirstBit++;
        }
    }

    if (ExtendSignBit)
    {
        uint8 SignBit = (uint8) ApplyMask(val, FirstBit, FirstBit, true, false);

        if (SignBit)
            for (int32 iBit = FirstBit - 1; iBit >= 0; iBit--)
                val |= 0x80000000 >> iBit;
    }

    return val;
}

EOpCode GetOpCode(int val, uint32 address)
{
    int code = ApplyMask((uint32) val, 0,5);

    switch (code)
    {
    case 4:
        return psq_stx;
    case 8:
        return subfic;
    case 10:
        return cmpli;
    case 11:
        return cmpi;
    case 12:
        return addic;
    case 13:
        return addicR;
    case 14:
        return addi;
    case 15:
        return addis;
    case 16:
        return bc;
    case 18:
        return b;
    case 19:
    {
        int xoc = ApplyMask(val, 21, 30);
        if (xoc == 16) return bclr;
        if (xoc == 193) return crxor;
        if (xoc == 449) return cror;
        if (xoc == 528) return bcctr;
        break;
    }
    case 20:
        return rlwimi;
    case 21:
        return rlwinm;
    case 24:
        return ori;
    case 25:
        return oris;
    case 27:
        return xoris;
    case 31:
    {
        int xoc = ApplyMask(val, 21, 30);
        if (xoc == 0)   return cmp;
        if (xoc == 24)  return slw;
        if (xoc == 40)  return subf;
        if (xoc == 339) return mfspr;
        if (xoc == 444) return or_;
        if (xoc == 467) return mtspr;
        if (xoc == 536) return srw;
        if (xoc == 954) return extsb;
        if ((xoc & 0x1FF) == 104) return neg;
        break;
    }
    case 32:
        return lwz;
    case 33:
        return lwzu;
    case 34:
        return lbz;
    case 36:
        return stw;
    case 37:
        return stwu;
    case 38:
        return stb;
    case 40:
        return lhz;
    case 44:
        return sth;
    case 46:
        return lmw;
    case 47:
        return stmw;
    case 48:
        return lfs;
    case 49:
        return lfsu;
    case 50:
        return lfd;
    case 52:
        return stfs;
    case 53:
        return stfsu;
    case 54:
        return stfd;
    case 56:
        return psq_l;
    case 59:
    {
        int xoc = ApplyMask(val, 26, 30);
        if (xoc == 18) return fdivs;
        if (xoc == 20) return fsubs;
        if (xoc == 21) return fadds;
        if (xoc == 25) return fmuls;
        if (xoc == 28) return fmsubs;
        if (xoc == 29) return fmadds;
        if (xoc == 30) return fnmsubs;
        break;
    }
    case 60:
        return psq_st;
    case 63:
    {
        int xoc = ApplyMask(val, 21, 30);
        if (xoc == 12) return frsp;
        if (xoc == 15) return fctiwz;
        if (xoc == 32) return fcmpo;
        if (xoc == 40) return fneg;
        if (xoc == 72) return fmr;
        if (xoc == 264) return fabs_;
        xoc &= 0x1F;
        if (xoc == 18) return fdiv;
        if (xoc == 20) return fsub;
        if (xoc == 21) return fadd;
        if (xoc == 23) return fsel;
        if (xoc == 25) return fmul;
        if (xoc == 26) return frsqrte;
        if (xoc == 28) return fmsub;
        if (xoc == 29) return fmadd;
        if (xoc == 30) return fnmsub;
        break;
    }
    }

    errorf("Ran into unknown opcode %d at 0x%08X", code, address);
    return UnknownOpCode;
}
