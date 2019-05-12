#ifndef CBYTECODEREADER_H
#define CBYTECODEREADER_H

#include "OpCodes.h"
#include "CStack.h"
#include <Common/BasicTypes.h>
#include <Common/FileIO.h>
#include <Common/TString.h>
#include <QMap>
#include <QString>

struct SModuleInfo
{
    QString Name;
    uint32 Address;

    SModuleInfo(const QString& kName, uint32 Addr) : Name(kName), Address(Addr) {}
};

struct SSection
{
    TString Name;
    uint32 Offset;
    uint32 Address;
    uint32 Size;
};

struct SInstruction
{
    EOpCode OpCode;
    uint32 Parameters;
};

struct SReaderState
{
    uint32 Address;
    uint32 Registers[32];
    float FloatRegisters[32];
    uint32 LinkRegister;
    QVector<char> Stack;
};

class CByteCodeReader
{
    // File Info
    CMemoryInStream mInput;
    SSection mSections[18];
    SSection mBssSection;
    uint32 mSectionAddresses[14]; // This is indexed by section ID, not file index
    uint32 mEntryPointAddress;
    uint32 mSectionsArrayAddress;
    QList<SModuleInfo> mModules;

    // Memory
    QVector<char> mMemory;
    uint32 mInitialStackAddress;
    static const uint32 mskStackSpace = 500000;
    static const uint32 mskArgSpace = 0x100;

    // State
    uint32 mRegisters[32];
    double mFloatRegisters[32];
    uint32 mLinkRegister;
    uint32 mCountRegister;
    uint32 mConditionRegister;

    uint32 mCurrentAddress;

public:
    CByteCodeReader();
    ~CByteCodeReader();

    // Init
    bool LoadDol(IInputStream& Dol);
    void LoadLinkedObjects(const TString& kDir, bool LoadProductionModulesOnly);
    void SetSectionAddress(uint32 ID, uint32 Address);
    uint32 SectionAddress(uint32 ID);

    // Registers
    void InitRegisters();
    uint32 GetRegisterValue(uint32 regID);
    double GetFloatRegisterValue(uint32 regID);
    void SetRegisterValue(uint32 regID, uint32 value);

    // File Nav
    bool GoToAddress(uint32 address, bool SetLinkRegister = false);
    void BranchReturn();
    uint32 OffsetForAddress(uint32 address);
    uint32 CurrentAddress();
    QString FindAddressModule(uint32 Address);

    // Code Reading/Execution
    SInstruction ParseInstruction();
    bool ExecuteInstruction(const SInstruction& kInstruction);
    void ExecuteFunction();
    void ExecuteFunction(uint32 address);
    void TestCondition(uint32 cr, int32 A, int32 B);
    void TestCondition(uint32 cr, double A, double B);
    bool CheckConditionalBranch(bool Condition, uint32 BO);

    // Memory
    void Store(uint32 address, void *pData, uint32 size);
    void Load(uint32 address, void *pOut, uint32 size);
    char* PointerToAddress(uint32 address);
    uint32 GetBssAddress();
    void DumpMemory(IOutputStream& Output);

    // State
    SReaderState SaveState();
    void LoadState(const SReaderState& State);
};

#endif // CBYTECODEREADER_H
