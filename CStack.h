#ifndef CSTACK_H
#define CSTACK_H

#include <Common/BasicTypes.h>
#include <QVector>

class CStack
{
    // Note: Unsafe to maintain access to a pointer from this class while executing.
    static const uint mskArgMemory = 0x100;

    uint32 mBaseAddress;
    uint32 mMinAddress;
    uint32 mMaxAddress;
    QVector<char> mStackMemory;

public:
    CStack()
        : mBaseAddress(0), mMinAddress(0), mMaxAddress(0)
    {
        Reset();
    }

    uint32 BaseAddress()
    {
        return mBaseAddress;
    }

    uint32 StackSize()
    {
        return mStackMemory.size();
    }

    void SetBaseAddress(uint32 addr)
    {
        mBaseAddress = addr;
        mMaxAddress = mBaseAddress + mskArgMemory;
        mMinAddress = mMaxAddress - StackSize();
    }

    void SetStackPointer(uint32 addr)
    {
        // Resize the stack as needed based on the pointer. Slow but don't care.
        if (addr != mMinAddress)
        {
            int32 diff = mMinAddress - addr;
            mMinAddress = addr;

            if (diff > 0)
                mStackMemory.insert(0, diff, (char) 0x80);
            if (diff < 0)
                mStackMemory.remove(0, -diff);
        }
    }

    bool IsStackAddress(uint32 addr)
    {
        return (addr >= mMinAddress && addr < mMaxAddress);
    }

    char* BasePointer()
    {
        char *pData = mStackMemory.data();
        return pData + StackSize() - mskArgMemory;
    }

    char* PointerToAddress(uint32 addr)
    {
        if (!IsStackAddress(addr)) return nullptr;

        int32 diff = addr - mBaseAddress;
        return BasePointer() + diff;
    }

    void Reset()
    {
        mStackMemory.clear();
        mStackMemory.resize(mskArgMemory);
        mStackMemory.fill((char) 0x80);
        mMinAddress = mBaseAddress;
        mMaxAddress = mBaseAddress + mskArgMemory;
    }

    void SaveState(QVector<char>& Out)
    {
        Out = mStackMemory;
    }

    void LoadState(const QVector<char>& In)
    {
        mStackMemory = In;
        mMinAddress = (mBaseAddress + mskArgMemory) - mStackMemory.size();
        mMaxAddress = mBaseAddress + mskArgMemory;
    }
};

#endif // CSTACK_H
