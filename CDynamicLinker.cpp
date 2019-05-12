#include "CDynamicLinker.h"
#include <iostream>

void CDynamicLinker::LoadSel(IInputStream& Sel)
{
    Sel.Seek(0x40, SEEK_SET);
    uint32 ExportsOffset = Sel.ReadLong();
    uint32 ExportsLength = Sel.ReadLong();
    uint32 ExportsNames = Sel.ReadLong();

    Sel.Seek(ExportsOffset, SEEK_SET);
    uint32 NumExports = ExportsLength / 0x10;

    for (uint32 i=0; i<NumExports; i++)
    {
        SSymbol Symbol;
        ReadSymbol(Symbol, Sel, ExportsNames, false);

        QString SymName = QString::fromStdString(Symbol.Name.ToStdString());
        mDolSymbols << SymName;
        mDolExports[SymName] = Symbol;
    }
}

void CDynamicLinker::LinkRELs(const QVector<QString>& kModuleFilenames, QList<SModuleInfo>& ModuleInfoOut)
{
    foreach (const QString& kFile, kModuleFilenames)
    {
        CFileInStream Input(kFile.toStdString(), EEndian::BigEndian);

        if (Input.IsValid())
        {
            uint32 Address = LoadREL(Input);
            ModuleInfoOut << SModuleInfo { kFile, Address };
        }
    }

    // Relocate
    QMapIterator<uint32,SModuleREL> ModIter(mModules);

    while (ModIter.hasNext())
    {
        ModIter.next();
        uint32 ModuleID = ModIter.key();
        const SModuleREL& kModule = ModIter.value();
        debugf("Linking %s", *kModule.ModuleName);

        for (int ImportIdx = 0; ImportIdx < kModule.Imports.size(); ImportIdx++)
        {
            const SImportREL& kImport = kModule.Imports[ImportIdx];
            uint32 CurrentAddress = 0;

            for (int RelocIdx = 0; RelocIdx < kImport.Relocations.size(); RelocIdx++)
            {
                const SRelocationREL& kReloc = kImport.Relocations[RelocIdx];
                CurrentAddress += kReloc.Offset;

                if (kReloc.Type == eDolphinSection)
                {
                    CurrentAddress = RelSectionAddress(ModuleID, kReloc.SectionID);
                }

                else if (kReloc.Type != eDolphinNop)
                {
                    uint32 Base = (kReloc.SectionID == 0 ? 0 : RelSectionAddress(kImport.ModuleID, kReloc.SectionID));

                    bool Success = Relocate(CurrentAddress, Base + kReloc.AddEnd, kReloc.Type);
                    if (!Success)
                        warnf("Unsupported reloc type %d", kReloc.Type);
                }
            }
        }
    }

    // Run prologs
    ModIter.toFront();

    while (ModIter.hasNext())
    {
        const SModuleREL& kModule = ModIter.next().value();
        mpReader->ExecuteFunction(kModule.PrologAddress);
    }
}

void CDynamicLinker::LinkRSOs(const QVector<QString>& kModuleFilenames, QList<SModuleInfo>& ModuleInfoOut)
{
    foreach (const QString& kFile, kModuleFilenames)
    {
        CFileInStream Input(kFile.toStdString(), EEndian::BigEndian);

        if (Input.IsValid())
        {
            uint32 Address = LinkRSO(Input);
            ModuleInfoOut << SModuleInfo { kFile, Address };
        }
    }
}

uint32 CDynamicLinker::LinkRSO(IInputStream& RSO)
{
    debugf("Linking %s", *RSO.GetSourceString().GetFileName());

    // Read
    mSections.clear();
    mInternals.clear();
    mExternals.clear();
    mExports.clear();
    mImports.clear();
    uint32 RsoAddress = LoadRSO(RSO);

    // Relocate internals
    for (int RelocIdx = 0; RelocIdx < mInternals.size(); RelocIdx++)
    {
        SRelocationRSO& Reloc = mInternals[RelocIdx];
        uint32 InstructionAddress = RsoAddress + Reloc.Offset;
        uint32 RelocAddress = mSections[Reloc.ID].Address + Reloc.Address;

        bool success = Relocate(InstructionAddress, RelocAddress, Reloc.Type);
        if (!success)
            errorf("Unsupported reloc type %d", Reloc.Type);
    }

    // Relocate externals
    for (int RelocIdx = 0; RelocIdx < mExternals.size(); RelocIdx++)
    {
        SRelocationRSO& Reloc = mExternals[RelocIdx];
        uint32 InstructionAddress = RsoAddress + Reloc.Offset;

        QString SymName = QString::fromStdString(mImports[Reloc.ID].Name.ToStdString());
        SSymbol Symbol = mDolExports[SymName];
        uint32 RelocAddress = Symbol.Address + mpReader->SectionAddress(Symbol.SectionID);

        bool success = Relocate(InstructionAddress, RelocAddress, Reloc.Type);
        if (!success)
            errorf("Unsupported reloc type %d", Reloc.Type);
    }

    // Run prolog
    mpReader->ExecuteFunction(mPrologAddress);
    return RsoAddress;
}

bool CDynamicLinker::Relocate(uint32 InstructionAddress, uint32 RelocAddress, ERelocType Type)
{
    switch (Type)
    {

    case eAddr16_Lo:
    {
        uint16 v = RelocAddress & 0xFFFF;
        mpReader->Store(InstructionAddress, &v, 2);
        break;
    }

    case eAddr16_Hi:
    {
        uint16 v = (RelocAddress >> 16) & 0xFFFF;
        mpReader->Store(InstructionAddress, &v, 2);
        break;
    }

    case eAddr16_Ha:
    {
        if (RelocAddress & 0x8000)
            RelocAddress += 0x10000;

        uint16 v = (RelocAddress >> 16) & 0xFFFF;
        mpReader->Store(InstructionAddress, &v, 2);
        break;
    }

    case eRel24:
    {
        uint32 Instruction;
        mpReader->Load(InstructionAddress, &Instruction, 4);
        uint32 RelAddress = RelocAddress - InstructionAddress;
        Instruction &= 0xFC000003;
        Instruction |= (RelAddress & 0x3FFFFFC);
        mpReader->Store(InstructionAddress, &Instruction, 4);
        break;
    }

    case eAddr32:
        mpReader->Store(InstructionAddress, &RelocAddress, 4);
        break;

    default:
        return false;
    }

    return true;
}

uint32 CDynamicLinker::RelSectionAddress(uint32 ModuleID, uint32 SectionID)
{
    if (ModuleID == 0) // DOL
        return mpReader->SectionAddress(SectionID);
    else if (mModules.contains(ModuleID))
        return mModules[ModuleID].Sections[SectionID].Address;
    else
        return 0;
}

uint32 CDynamicLinker::LoadRSO(IInputStream &rRSO)
{
    // Load RSO into memory
    uint32 RsoAddress = mMemoryPos;
    char *pOut = mpReader->PointerToAddress(RsoAddress);
    rRSO.ReadBytes(pOut, rRSO.Size());
    mMemoryPos += rRSO.Size();

    // Header
    rRSO.Seek(0x8, SEEK_SET);
    uint32 NumSections = rRSO.ReadLong();
    uint32 SectionsOffset = rRSO.ReadLong();
    rRSO.Seek(0x20, SEEK_SET);
    mPrologSection = rRSO.ReadByte();
    rRSO.Seek(0x3, SEEK_CUR);
    mPrologAddress = rRSO.ReadLong();
    rRSO.Seek(0x30, SEEK_SET);
    uint32 InternalsOffset = rRSO.ReadLong();
    uint32 InternalsLength = rRSO.ReadLong();
    uint32 ExternalsOffset = rRSO.ReadLong();
    uint32 ExternalsLength = rRSO.ReadLong();
    uint32 ExportsOffset = rRSO.ReadLong();
    uint32 ExportsLength = rRSO.ReadLong();
    uint32 ExportsNames = rRSO.ReadLong();
    uint32 ImportsOffset = rRSO.ReadLong();
    uint32 ImportsLength = rRSO.ReadLong();
    uint32 ImportsNames = rRSO.ReadLong();

    // Sections
    mSections.resize(NumSections);
    rRSO.Seek(SectionsOffset, SEEK_SET);

    for (uint32 SecIdx = 0; SecIdx < NumSections; SecIdx++)
    {
        mSections[SecIdx].Offset = rRSO.ReadLong();
        mSections[SecIdx].Size = rRSO.ReadLong();

        // Initialize BSS
        if ((mSections[SecIdx].Offset == 0) && (mSections[SecIdx].Size != 0))
        {
            pOut = mpReader->PointerToAddress(mMemoryPos);
            memset(pOut, 0, mSections[SecIdx].Size);
            mSections[SecIdx].Address = mMemoryPos;
            mMemoryPos += mSections[SecIdx].Size;
        }

        else
            mSections[SecIdx].Address = mSections[SecIdx].Offset + RsoAddress;
    }

    mPrologAddress += mSections[mPrologSection].Address;

    // Internals
    uint32 NumInternals = InternalsLength / 0xC;
    mInternals.resize(NumInternals);
    rRSO.Seek(InternalsOffset, SEEK_SET);

    for (uint32 ReloxIdx = 0; ReloxIdx < NumInternals; ReloxIdx++)
        ReadRelocation(mInternals[ReloxIdx], rRSO);

    // Externals
    uint32 NumExternals = ExternalsLength / 0xC;
    mExternals.resize(NumExternals);
    rRSO.Seek(ExternalsOffset, SEEK_SET);

    for (uint32 RelocIdx = 0; RelocIdx < NumExternals; RelocIdx++)
        ReadRelocation(mExternals[RelocIdx], rRSO);

    // Exports
    uint32 NumExports = ExportsLength / 0x10;
    mExports.resize(NumExports);
    rRSO.Seek(ExportsOffset, SEEK_SET);

    for (uint32 ExportIdx = 0; ExportIdx < NumExports; ExportIdx++)
        ReadSymbol(mExports[ExportIdx], rRSO, ExportsNames, false);

    // Imports
    uint32 NumImports = ImportsLength / 0xC;
    mImports.resize(NumImports);
    rRSO.Seek(ImportsOffset, SEEK_SET);

    for (uint32 ImportIdx = 0; ImportIdx < NumImports; ImportIdx++)
        ReadSymbol(mImports[ImportIdx], rRSO, ImportsNames, true);

    // Return RSO address
    return RsoAddress;
}

void CDynamicLinker::ReadRelocation(SRelocationRSO& Reloc, IInputStream& File)
{
    Reloc.Offset = File.ReadLong();
    uint32 Val = File.ReadLong();
    Reloc.ID = (Val >> 8) & 0x00FFFFFF;
    Reloc.Type = (ERelocType) (Val & 0x000000FF);
    Reloc.Address = File.ReadLong();
}

void CDynamicLinker::ReadSymbol(SSymbol& Symbol, IInputStream& File, uint32 NamesOffset, bool IsImport)
{
    uint32 SymNameOffset = File.ReadLong();
    Symbol.Address = File.ReadLong();
    Symbol.SectionID = File.ReadLong();
    if (!IsImport) File.Seek(0x4, SEEK_CUR); // Skipping ELF Hash

    uint32 cur = File.Tell();
    File.Seek(NamesOffset + SymNameOffset, SEEK_SET);
    Symbol.Name = File.ReadString();
    File.Seek(cur, SEEK_SET);
}

uint32 CDynamicLinker::LoadREL(IInputStream& REL)
{
    // Check module ID
    REL.Seek(0x0, SEEK_SET);
    uint32 ModuleID = REL.ReadLong();
    if (mModules.contains(ModuleID)) return -1;

    // Load REL into memory
    uint32 RelAddress = mMemoryPos;
    char *pOut = mpReader->PointerToAddress(RelAddress);
    REL.Seek(0x0, SEEK_SET);
    REL.ReadBytes(pOut, REL.Size());
    mMemoryPos += REL.Size();

    // Header
    REL.Seek(0xC, SEEK_SET);
    uint32 NumSections = REL.ReadLong();
    uint32 SectionsOffset = REL.ReadLong();
    REL.Seek(0x28, SEEK_SET);
    uint32 ImportsOffset = REL.ReadLong();
    uint32 ImportsLength = REL.ReadLong();
    uint8 PrologSection = REL.ReadByte();
    REL.Seek(0x3, SEEK_CUR);
    uint32 PrologAddress = REL.ReadLong();

    // Create Module
    SModuleREL& Module = mModules[ModuleID];
    Module.ModuleName = TString(REL.GetSourceString()).GetFileName();
    Module.ModuleID = ModuleID;
    Module.PrologSection = PrologSection;
    Module.PrologAddress = PrologAddress;

    // Sections
    Module.Sections.resize(NumSections);
    REL.Seek(SectionsOffset, SEEK_SET);

    for (uint32 SecIdx = 0; SecIdx < NumSections; SecIdx++)
    {
        SSection *pSec = &Module.Sections[SecIdx];
        pSec->Offset = REL.ReadLong() & ~0x1;
        pSec->Size = REL.ReadLong();

        // Initialize BSS
        if ((pSec->Offset == 0) && (pSec->Size != 0))
        {
            pOut = mpReader->PointerToAddress(mMemoryPos);
            memset(pOut, 0, pSec->Size);
            pSec->Address = mMemoryPos;
            mMemoryPos += pSec->Size;
        }

        else
            pSec->Address = pSec->Offset + RelAddress;
    }

    Module.PrologAddress += Module.Sections[Module.PrologSection].Address;

    // Imports
    uint32 NumImports = ImportsLength / 0x8;
    Module.Imports.resize(NumImports);
    REL.Seek(ImportsOffset, SEEK_SET);

    for (uint32 ImportIdx = 0; ImportIdx < NumImports; ImportIdx++)
    {
        Module.Imports[ImportIdx].ModuleID = REL.ReadLong();
        Module.Imports[ImportIdx].Offset = REL.ReadLong();
    }

    // Relocations
    for (uint32 ImportIdx = 0; ImportIdx < NumImports; ImportIdx++)
    {
        SImportREL *pImport = &Module.Imports[ImportIdx];
        REL.Seek(pImport->Offset, SEEK_SET);

        while (true)
        {
            SRelocationREL Reloc;
            Reloc.Offset = REL.ReadShort();
            Reloc.Type = (ERelocType) (uint8) REL.ReadByte();
            Reloc.SectionID = REL.ReadByte();
            Reloc.AddEnd = REL.ReadLong();

            if (Reloc.Type == eDolphinEnd) break;
            else pImport->Relocations << Reloc;
        }
    }

    return RelAddress;
}
