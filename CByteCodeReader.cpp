#include "CByteCodeReader.h"
#include "CDynamicLinker.h"
#include "Masks.h"
#include <iostream>
#include <QDir>
#include <QFile>

CByteCodeReader::CByteCodeReader()
{
    for (int i=0; i<7; i++)
        mSections[i].Name = ".text" + TString::FromInt32(i, 0, 10);

    for (int i=0; i<11; i++)
        mSections[i+7].Name = ".data" + TString::FromInt32(i, 0, 10);

    for (int i=0; i<14; i++)
        mSectionAddresses[i] = 0;

    mBssSection.Name = ".bss";

    mInitialStackAddress = 0;

    mLinkRegister = 0;
    mCountRegister = 0;
    mConditionRegister = 0;

    mSectionsArrayAddress = 0;
}

CByteCodeReader::~CByteCodeReader()
{
}

bool CByteCodeReader::LoadDol(IInputStream& Dol)
{
    if (!Dol.IsValid()) return false;

    // Section Offsets
    for (int i=0; i<18; i++)
        mSections[i].Offset = Dol.ReadLong();

    // Section Addresses
    for (int i=0; i<18; i++)
        mSections[i].Address = Dol.ReadLong();

    // Section Sizes
    for (int i=0; i<18; i++)
        mSections[i].Size = Dol.ReadLong();

    // BSS and entry point
    mBssSection.Address = Dol.ReadLong();
    mBssSection.Size = Dol.ReadLong();
    mEntryPointAddress = Dol.ReadLong();

    // Init memory
    mMemory.clear();
    mMemory.resize(24000000);
    mMemory.fill((char) 0x80); // Using 0x80 as a simple way to see uninitialized memory
    mInput.SetData(mMemory.data(), mMemory.size(), EEndian::BigEndian);
    memset(PointerToAddress(mBssSection.Address), 0, mBssSection.Size);

    // Load each section into memory
    for (int i=0; i<18; i++)
    {
        SSection *pSec = &mSections[i];
        char *pSecPtr = PointerToAddress(pSec->Address);
        Dol.Seek(pSec->Offset, SEEK_SET);
        Dol.ReadBytes(pSecPtr, pSec->Size);
    }

    mModules << SModuleInfo("DOL", 0x80000000);
    InitRegisters();
    return true;
}

void CByteCodeReader::LoadLinkedObjects(const TString& kDir, bool LoadProductionModulesOnly)
{
    // Create linker
    static const uint32 skDynamicLinkAddress = 0x80800000;
    CDynamicLinker Linker(this, skDynamicLinkAddress);

    // Load SEL
    QDir dir(QString::fromStdString(kDir.ToStdString()));

    QStringList SelFilter("*.sel");
    QFileInfoList SelEntries = dir.entryInfoList(SelFilter, QDir::Files | QDir::NoDotAndDotDot);

    if (!SelEntries.isEmpty())
    {
        TString path = SelEntries[0].absoluteFilePath().toStdString();
        CFileInStream sel(path.ToStdString(), EEndian::BigEndian);

        if (!sel.IsValid())
        {
            errorf("Couldn't open sel");
            return;
        }
        Linker.LoadSel(sel);
    }

    // Load RSO and REL
    QStringList filters;
    filters << "*.rso" << "*.rel";
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

    QVector<QString> RelNames;
    QVector<QString> RsoNames;

    foreach (const QFileInfo& info, entries)
    {
        if (info.completeSuffix() == "rso")
            RsoNames << info.absoluteFilePath();
        else
        {
            // This is for MP2 demo which contains both debug and production variants of modules
            if (LoadProductionModulesOnly && !info.fileName().endsWith("P.rel")) continue;
            RelNames << info.absoluteFilePath();
        }
    }

    if (!RelNames.isEmpty())
        Linker.LinkRELs(RelNames, mModules);
    if (!RsoNames.isEmpty())
        Linker.LinkRSOs(RsoNames, mModules);
}

void CByteCodeReader::InitRegisters()
{
    GoToAddress(mEntryPointAddress);

    // Execute the game's __init_registers function, which is the first branch in main
    while (true)
    {
        SInstruction Instruction = ParseInstruction();

        if (Instruction.OpCode == b)
        {
            ExecuteInstruction(Instruction);
            break;
        }
    }

    // Initialize floating point registers and set up stack memory
    for (int i=0; i<32; i++)
        mFloatRegisters[i] = 0.0f;

    mInitialStackAddress = mRegisters[1];
}

void CByteCodeReader::SetSectionAddress(uint32 ID, uint32 Address)
{
    mSectionAddresses[ID] = Address;
}

uint32 CByteCodeReader::SectionAddress(uint32 ID)
{
    return mSectionAddresses[ID];
}

uint32 CByteCodeReader::GetRegisterValue(uint32 regID)
{
    return mRegisters[regID];
}

double CByteCodeReader::GetFloatRegisterValue(uint32 regID)
{
    return mFloatRegisters[regID];
}

void CByteCodeReader::SetRegisterValue(uint32 regID, uint32 value)
{
    mRegisters[regID] = value;
}

bool CByteCodeReader::GoToAddress(uint32 address, bool SetLinkRegister /*= false*/)
{
    if (SetLinkRegister) mLinkRegister = mCurrentAddress;
    mInput.Seek(OffsetForAddress(address), SEEK_SET);
    mCurrentAddress = address;
    return true;
}

void CByteCodeReader::BranchReturn()
{
    GoToAddress(mLinkRegister);
}

uint32 CByteCodeReader::OffsetForAddress(uint32 address)
{
    return address & 0x0FFFFFFF;
}

uint32 CByteCodeReader::CurrentAddress()
{
    return mCurrentAddress - 4;
}

QString CByteCodeReader::FindAddressModule(uint32 Address)
{
    for (int iMod = 0; iMod < mModules.size(); iMod++)
    {
        bool IsLast = (iMod == mModules.size() - 1);
        if (Address >= mModules[iMod].Address && (IsLast || Address < mModules[iMod + 1].Address))
            return mModules[iMod].Name;
    }

    return "UNKNOWN MODULE";
}

SInstruction CByteCodeReader::ParseInstruction()
{
    SInstruction out;
    out.Parameters = mInput.ReadLong();
    out.OpCode = GetOpCode(out.Parameters, mCurrentAddress);
    mCurrentAddress += 4;
    return out;
}

bool CByteCodeReader::ExecuteInstruction(const SInstruction& kInstruction)
{
    uint32 Params = kInstruction.Parameters;

    switch (kInstruction.OpCode)
    {
    case addi: // Add Immediate
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 SIMM = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rD] = (rA == 0 ? 0 : mRegisters[rA]) + SIMM;
        break;
    }

    case addic: // Add Immediate Carrying
    case addicR: // Add Immediate Carrying and Record
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 SIMM = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rD] = mRegisters[rA] + SIMM;

        if (kInstruction.OpCode == addicR)
            TestCondition(0, (int32) mRegisters[rD], 0);

        break;
    }

    case addis: // Add Immediate Shifted
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 SIMM = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rD] = (rA == 0 ? 0 : mRegisters[rA]) + (SIMM << 16);
        break;
    }

    case b: // Branch
    {
        int32 TargetAddr = ApplyMask(Params, 6, 29, false, true);
        bool AA = ApplyMask(Params, 30, 30) == 1;
        bool LK = ApplyMask(Params, 31, 31) == 1;

        if (!AA) TargetAddr += mCurrentAddress - 4;
        if (LK)  mLinkRegister = mCurrentAddress;

        if (LK) ExecuteFunction(TargetAddr);
        else GoToAddress(TargetAddr);
        break;
    }

    case bc: // Branch Conditional
    {
        uint32 BO = ApplyMask(Params, 6, 10);
        uint32 BI = ApplyMask(Params, 11, 15);
        int16 BD = (int16) ApplyMask(Params, 16, 29, false, true);
        bool AA = ApplyMask(Params, 30, 30) == 1;
        bool LK = ApplyMask(Params, 31, 31) == 1;

        uint32 TargetAddr = (AA ? (uint32) BD : CurrentAddress() + BD);
        bool Condition = ApplyMask(mConditionRegister, BI, BI) == 1;
        bool ShouldBranch = CheckConditionalBranch(Condition, BO);

        if (ShouldBranch)
        {
            if (LK)
            {
                mLinkRegister = mCurrentAddress;
                ExecuteFunction(TargetAddr);
            }
            else GoToAddress(TargetAddr);
        }
        break;
    }

    case bcctr: // Branch Conditional to Count Register
    {
        uint32 BO = ApplyMask(Params, 6, 10);
        uint32 BI = ApplyMask(Params, 11, 15);
        bool LK = ApplyMask(Params, 31, 31) == 1;

        bool Condition = ApplyMask(mConditionRegister, BI, BI) == 1;
        bool ShouldBranch = CheckConditionalBranch(Condition, BO);

        if (ShouldBranch)
        {
            if (LK)
            {
                mLinkRegister = mCurrentAddress;
                ExecuteFunction(mCountRegister);
            }
            else GoToAddress(mCountRegister);
        }
        break;
    }

    case bclr: // Branch Conditional to Link Register
    {
        uint32 BO = ApplyMask(Params, 6, 10);
        bool LK = ApplyMask(Params, 31, 31) == 1;

        uint32 TargetAddr = mLinkRegister;
        if (LK) mLinkRegister = mCurrentAddress;

        if (BO & 0x14) GoToAddress(TargetAddr);
        else errorf("bclr: unsupported BO value: %d", BO);
        return true;
    }

    case cmp: // Compare
    {
        //debugf("Executing cmp at 0x%08X", CurrentAddress());
        uint32 BF = ApplyMask(Params, 6, 8);
        uint32 rA = ApplyMask(Params, 11, 15);
        uint32 rB = ApplyMask(Params, 16, 20);
        TestCondition(BF, (int32) mRegisters[rA], (int32) mRegisters[rB]);
        break;
    }

    case cmpli: // Compare Logical Immediate
    {
        //debugf("Executing cmpli at 0x%08X"), CurrentAddress());
        uint32 BF = ApplyMask(Params, 6, 8);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 UI = (int16) ApplyMask(Params, 16, 31);
        TestCondition(BF, (int32) mRegisters[rA], (int32) UI);
        break;
    }

    case cmpi: // Compare Immediate
    {
        uint32 crfD = ApplyMask(Params, 6, 8);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 SIMM = (int16) ApplyMask(Params, 16, 31);
        TestCondition(crfD, (int32) mRegisters[rA], (int32) SIMM);
        break;
    }

    case cror: // Condition Register OR
    {
        uint32 crbD = ApplyMask(Params, 6, 10);
        uint32 crbA = ApplyMask(Params, 11, 15);
        uint32 crbB = ApplyMask(Params, 16, 20);

        uint8 b1 = ApplyMask(mConditionRegister, crbA, crbA);
        uint8 b2 = ApplyMask(mConditionRegister, crbB, crbB);
        uint8 bb = b1 | b2;
        mConditionRegister |= (bb << (31 - crbD));
        break;
    }

    case crxor: // Condition Register XOR
    {
        // not supported
        break;
    }

    case extsb: // Extend Sign Bit
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        bool Rc = ApplyMask(Params, 31, 31) == 1;

        mRegisters[rA] = (uint32) ((uint8) mRegisters[rS]);
        uint8 sign = mRegisters[rA] & 0x80;

        if (sign)
            for (uint32 Bit = 0x100; Bit <= 0x80000000; Bit <<= 1)
                mRegisters[rA] |= Bit;

        if (Rc) TestCondition(0, (int32) mRegisters[rA], 0);
        break;
    }

    case fabs_: // Floating Absolute Value
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fabs wants to record");

        mFloatRegisters[frT] = fabs(mFloatRegisters[frB]);
        break;
    }

    case fadd: // Floating Add
    case fadds: // Floating Add Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fadds wants to record");

        mFloatRegisters[frT] = mFloatRegisters[frA] + mFloatRegisters[frB];
        if (kInstruction.OpCode == fadds) mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];
        break;
    }

    case fcmpo: // Floating Compare Ordered
    {
        uint32 BF = ApplyMask(Params, 6, 8);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);

        TestCondition(BF, mFloatRegisters[frA], mFloatRegisters[frB]);
        break;
    }

    case fctiwz: // Floating Convert to Integer Word with Round Toward Zero
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fctiwz wants to record");

        double src = mFloatRegisters[frB];
        if (src > 2147483647.0) src = 2147483647.0;
        if (src < -2147483648.0) src = -2147483648.0;
        int32 v = (int32) mFloatRegisters[frB];

        memcpy(&mFloatRegisters[frT], &v, 4);
        break;
    }

    case fdiv: // Floating Divide
    case fdivs: // Floating Divide Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fdiv wants to record");

        mFloatRegisters[frT] = mFloatRegisters[frA] / mFloatRegisters[frB];
        if (kInstruction.OpCode == fdivs)
            mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];

        break;
    }

    case fmadd: // Floating Multiply-Add
    case fmadds: // Floating Multiply-Add Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        uint32 frC = ApplyMask(Params, 21, 25);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fmadds wants to record");

        mFloatRegisters[frT] = (mFloatRegisters[frA] * mFloatRegisters[frC]) + mFloatRegisters[frB];

        if (kInstruction.OpCode == fmadds)
            mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];

        break;
    }

    case fmr: // Floating Move Register
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fmr wants to record");
        mFloatRegisters[frT] = mFloatRegisters[frB];
        break;
    }

    case fmsub: // Floating Multiply Subtract
    case fmsubs: // Floating Multiply Subtract Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        uint32 frC = ApplyMask(Params, 21, 25);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fmsub wants to record");

        mFloatRegisters[frT] = (mFloatRegisters[frA] * mFloatRegisters[frC]) - mFloatRegisters[frB];

        if (kInstruction.OpCode == fmsubs)
            mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];

        break;
    }

    case fmul: // Floating Multiply
    case fmuls: // Floating Multiply Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frC = ApplyMask(Params, 21, 25);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fmul wants to record");

        mFloatRegisters[frT] = mFloatRegisters[frA] * mFloatRegisters[frC];
        if (kInstruction.OpCode == fmuls)
            mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];

        break;
    }

    case fneg: // Floating Negate
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frB = ApplyMask(Params, 16, 20);
        mFloatRegisters[frT] = -mFloatRegisters[frB];
        break;
    }

    case fnmsub: // Floating Negative Multiply-Subtract
    case fnmsubs: // Floating Negative Multiply-Subtract Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        uint32 frC = ApplyMask(Params, 21, 25);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fnmsubs wants to record");
        mFloatRegisters[frT] = -((mFloatRegisters[frA] * mFloatRegisters[frC]) - mFloatRegisters[frB]);

        if (kInstruction.OpCode == fnmsubs)
            mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];

        break;
    }

    case frsp: // Floating-Point Round to Single Precision
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("frsp wants to record");
        mFloatRegisters[frT] = (double) (float) mFloatRegisters[frB];
        break;
    }

    case frsqrte: // Floating Reciprocal Square Root Estimate
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("frsqrte wants to record");

        mFloatRegisters[frT] = sqrt(mFloatRegisters[frB]);
        break;
    }

    case fsel: // Floating Select
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        uint32 frC = ApplyMask(Params, 21, 25);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fsel wants to record");

        if (mFloatRegisters[frA] >= -0.0)
            mFloatRegisters[frT] = mFloatRegisters[frC];
        else
            mFloatRegisters[frT] = mFloatRegisters[frB];

        break;
    }

    case fsub: // Floating Subtract
    case fsubs: // Floating Subtract Single
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 frA = ApplyMask(Params, 11, 15);
        uint32 frB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("fsubs wants to record");

        mFloatRegisters[frT] = (double) (float) (mFloatRegisters[frA] - mFloatRegisters[frB]);
        if (kInstruction.OpCode == fsubs)
            mFloatRegisters[frT] = (double) (float) mFloatRegisters[frT];

        break;
    }

    case lbz: // Load Byte and Zero
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        uint8 byte;
        Load(EA, &byte, 1);
        mRegisters[rD] = (uint32) byte;
        break;
    }

    case lfd: // Load Floating Point Double
    case psq_l: // Paired Single Quantize Load
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        Load(EA, &mFloatRegisters[frT], 8);
        break;
    }

    case lfs: // Load Floating Point Single
    case lfsu: // Load Floating Point Single with Update
    {
        uint32 frT = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        float f;
        uint32 EA = mRegisters[rA] + d;
        Load(EA, &f, 4);
        mFloatRegisters[frT] = (double) f;

        if (kInstruction.OpCode == lfsu)
            mRegisters[rA] = EA;

        break;
    }

    case lhz: // Load Half Word and Zero
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        int16 v;
        Load(EA, &v, 2);
        mRegisters[rD] = (uint32) v;
        break;
    }

    case lmw: // Load Multiple Word
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        uint32 NumWords = 32 - rD;

        for (uint32 i=0; i<NumWords; i++)
            Load(EA + (i * 4), &mRegisters[rD + i], 4);
        break;
    }

    case lwz: // Load Word and Zero
    case lwzu: // Load Word and Zero with Update
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        Load(EA, &mRegisters[rD], 4);

        if (kInstruction.OpCode == lwzu)
            mRegisters[rA] = EA;

        break;
    }

    case mfspr: // Move from Special-Purpose Register
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 spr = ApplyMask(Params, 11, 20);

        if (spr == 20)       warnf("mfspr wants XER");
        else if (spr == 256) mRegisters[rD] = mLinkRegister;
        else if (spr == 288) warnf("mfspr wants CTR");
        else                 warnf("mfspr wants invalid register: %d", spr);
        break;
    }

    case mtspr: // Move to Special-Purpose Register
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 spr = ApplyMask(Params, 11, 20);

        if (spr == 20)       warnf("mtspr wants XER");
        else if (spr == 256) mLinkRegister = mRegisters[rS];
        else if (spr == 288) mCountRegister = mRegisters[rS];
        else                 warnf("mtspr wants invalid register: %d", spr);
        break;
    }

    case neg: // Negate
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        //bool OE = ApplyMask(Params, 21, 21); // not handling
        bool Rc = ApplyMask(Params, 31, 31);

        mRegisters[rD] = ~mRegisters[rA] + 1;
        if (Rc) TestCondition(0, (int32) mRegisters[rD], 0);
        break;
    }

    case or_: // OR
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        uint32 rB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;

        if (Rc) warnf("or wants to record");
        mRegisters[rA] = mRegisters[rS] | mRegisters[rB];
        break;
    }

    case ori: // OR Immediate
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 UIMM = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rA] = mRegisters[rS] | (UIMM & 0xFFFF);
        break;
    }

    case oris: // OR Immediate Shifted
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 UIMM = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rA] = mRegisters[rS] | (UIMM << 16);
        break;
    }

    case psq_stx: // Paired Single Quantized Store Indexed
    {
        // Ignore this instruction
        warnf("Ignoring psq_stx");
        break;
    }

    case rlwimi: // Rotate Left Word Immediate then Mask Insert
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        uint32 SH = ApplyMask(Params, 16, 20);
        uint32 MB = ApplyMask(Params, 21, 25);
        uint32 ME = ApplyMask(Params, 26, 30);
        uint32 Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("rlwimi wants to record");

        uint32 v = mRegisters[rS];
        v = (v << SH) | (v >> (32 - SH));
        uint32 mask = CreateMask(MB, ME);
        mRegisters[rA] &= ~mask;
        mRegisters[rA] |= (v & mask);
        break;
    }

    case rlwinm: // Rotate Left Word Immediate then AND with Mask
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        uint32 SH = ApplyMask(Params, 16, 20);
        uint32 MB = ApplyMask(Params, 21, 25);
        uint32 ME = ApplyMask(Params, 26, 30);
        bool Rc = ApplyMask(Params, 31, 31) == 1;

        uint32 v = mRegisters[rS];
        v = (v << SH) | (v >> (32 - SH));
        v &= CreateMask(MB, ME);
        mRegisters[rA] = v;

        if (Rc) TestCondition(0, (int32) mRegisters[rA], 0);
        break;
    }

    case slw: // Shift Left Word
    case srw: // Shift Right Word
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        uint32 rB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;
        if (Rc) warnf("slw/srw wants to record");

        uint32 NumBits = mRegisters[rB] & 0x3F;
        if (kInstruction.OpCode == slw)
            mRegisters[rA] = (mRegisters[rS] << NumBits) & 0xFFFFFFFF;
        else
            mRegisters[rA] = (mRegisters[rS] >> NumBits) & 0xFFFFFFFF;
        break;
    }

    case stb: // Store Byte
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        uint8 byte = (uint8) (mRegisters[rS] & 0xFF);
        Store(EA, &byte, 1);
        break;
    }

    case stfd: // Store Floating Point Double
    case psq_st: // Paired Single Quantize Store
    {
        uint32 frS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        Store(EA, &mFloatRegisters[frS], 8);
        break;
    }

    case stfs: // Store Floating Point Single
    case stfsu: // Store Floating Point Single with Update
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        float f = (float) mFloatRegisters[rS];
        uint32 EA = mRegisters[rA] + d;
        Store(EA, &f, 4);

        if (kInstruction.OpCode == stfsu)
            mRegisters[rA] = EA;

        break;
    }

    case sth: // Store Half Word
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        uint16 v = (uint16) mRegisters[rS];
        Store(EA, &v, 2);
        break;
    }

    case stmw: // Store Multiple Word
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        uint32 NumWords = 32 - rS;

        for (uint32 i=0; i<NumWords; i++)
            Store(EA + (i*4), &mRegisters[rS+i], 4);
        break;
    }

    case stw: // Store Word
    case stwu: // Store Word and Update
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 d = (int16) ApplyMask(Params, 16, 31);

        uint32 EA = mRegisters[rA] + d;
        Store(EA, &mRegisters[rS], 4);

        if (kInstruction.OpCode == stwu)
            mRegisters[rA] = EA;

        break;
    }

    case subf: // Subtract From
    {
        uint32 rT = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        uint32 rB = ApplyMask(Params, 16, 20);
        bool Rc = ApplyMask(Params, 31, 31) == 1;

        int32 A = (int32) mRegisters[rA];
        int32 B = (int32) mRegisters[rB];
        mRegisters[rT] = (uint32) (~B + A + 1);

        if (Rc) TestCondition(0, (int32) mRegisters[rT], 0);
        break;
    }

    case subfic: // Subtract From Immediate Carrying
    {
        uint32 rD = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 SIMM = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rD] = (uint32) (~mRegisters[rA] + SIMM + 1);
        break;
    }

    case xoris: // XOR Immediate Shifted
    {
        uint32 rS = ApplyMask(Params, 6, 10);
        uint32 rA = ApplyMask(Params, 11, 15);
        int16 UI = (int16) ApplyMask(Params, 16, 31);
        mRegisters[rA] = mRegisters[rS] ^ (UI << 16);
        break;
    }

    default:
        return true;
    }

    return false;
}

void CByteCodeReader::ExecuteFunction()
{
    while (true)
    {
        SInstruction instr = ParseInstruction();
        bool FuncOver = ExecuteInstruction(instr);
        if (FuncOver) break;
    }
}

void CByteCodeReader::ExecuteFunction(uint32 address)
{
    GoToAddress(address);
    ExecuteFunction();
}

void CByteCodeReader::TestCondition(uint32 cr, int32 A, int32 B)
{
    uint32 v = 0;
    if (A < B)  v |= 0x80000000;
    if (A > B)  v |= 0x40000000;
    if (A == B) v |= 0x20000000;
    v >>= (cr * 4);

    uint32 CrMask = 0xF0000000 >> (cr * 4);
    mConditionRegister &= ~CrMask;
    mConditionRegister |= v;
}

void CByteCodeReader::TestCondition(uint32 cr, double A, double B)
{
    uint32 v = 0;
    if (A < B)  v |= 0x80000000;
    if (A > B)  v |= 0x40000000;
    if (A == B) v |= 0x20000000;
    v >>= (cr * 4);

    uint32 CrMask = 0xF0000000 >> (cr * 4);
    mConditionRegister &= ~CrMask;
    mConditionRegister |= v;
}

bool CByteCodeReader::CheckConditionalBranch(bool Condition, uint32 BO)
{
    switch (BO)
    {
    case 0: // Decrement CTR and branch if decremented CTR != 0 and condition is FALSE
        mCountRegister--;
        return (mCountRegister != 0) && !Condition;
    case 2: // Decrement CTR and branch if decremented CTR == 0 and condition is FALSE
        mCountRegister--;
        return (mCountRegister == 0) && !Condition;
    case 4: // Branch if condition is FALSE
        return !Condition;
    case 8: // Decrement CTR and branch if decremented CTR != 0 and condition is TRUE
        mCountRegister--;
        return (mCountRegister != 0) && Condition;
    case 10: // Decrement CTR and branch if decremented CTR == 0 and condition is TRUE
        mCountRegister--;
        return (mCountRegister == 0) && Condition;
    case 12: // Branch if condition is TRUE
        return Condition;
    case 16: // Decrement CTR and branch if decremented CTR != 0
        mCountRegister--;
        return (mCountRegister != 0);
    case 18: // Decrement CTR and branch if decremented CTR == 0
        mCountRegister--;
        return (mCountRegister == 0);
    case 20: // Branch always
        return true;
    default:
        errorf("Invalid BO value in conditional branch: %d", BO);
        return false;
    }
}

void CByteCodeReader::Store(uint32 address, void *pData, uint32 size)
{
    char *pOut = PointerToAddress(address);
    memcpy(pOut, pData, size);

    // Swap to big endian
    if (EEndian::SystemEndian == EEndian::LittleEndian)
    {
        if (size == 2) {
            SwapBytes(*((uint16*)(pOut)));
        } else if (size == 4) {
            SwapBytes(*((uint32*)(pOut)));
        } else if (size == 8) {
            SwapBytes(*((uint64*)(pOut)));
        }
    }
}

void CByteCodeReader::Load(uint32 address, void *pOut, uint32 size)
{
    char *pLoc = PointerToAddress(address);
    memcpy(pOut, pLoc, size);

    // Swap to little endian
    if (EEndian::SystemEndian == EEndian::LittleEndian)
    {
        if (size == 2) {
            SwapBytes(*((uint16*)(pOut)));
        } else if (size == 4) {
            SwapBytes(*((uint32*)(pOut)));
        } else if (size == 8) {
            SwapBytes(*((uint64*)(pOut)));
        }
    }
}

char* CByteCodeReader::PointerToAddress(uint32 address)
{
    address &= 0x0FFFFFFF;
    return mMemory.data() + address;
}

uint32 CByteCodeReader::GetBssAddress()
{
    return mBssSection.Address;
}

void CByteCodeReader::DumpMemory(IOutputStream& Output)
{
    Output.WriteBytes(mMemory.data(), mMemory.size());
}

SReaderState CByteCodeReader::SaveState()
{
    SReaderState State;
    State.Address = mCurrentAddress;
    State.LinkRegister = mLinkRegister;

    for (int i=0; i<32; i++)
    {
        State.Registers[i] = mRegisters[i];
        State.FloatRegisters[i] = mFloatRegisters[i];
    }

    State.Stack.resize(mskStackSpace);
    memcpy(State.Stack.data(), PointerToAddress(mInitialStackAddress) + mskArgSpace - mskStackSpace, mskStackSpace);
    return State;
}

void CByteCodeReader::LoadState(const SReaderState& State)
{
    GoToAddress(State.Address);
    mLinkRegister = State.LinkRegister;

    for (int i=0; i<32; i++)
    {
        mRegisters[i] = State.Registers[i];
        mFloatRegisters[i] = State.FloatRegisters[i];
    }

    memcpy(PointerToAddress(mInitialStackAddress) + mskArgSpace - mskStackSpace, State.Stack.data(), mskStackSpace);
}
