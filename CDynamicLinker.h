#ifndef CDYNAMICLINKER_H
#define CDYNAMICLINKER_H

#include <Common/BasicTypes.h>
#include "CByteCodeReader.h"
#include <QMap>
#include <QString>
#include <QVector>

enum ERelocType
{
    eNone = 0,
    eAddr32 = 1,
    eAddr24 = 2,
    eAddr16 = 3,
    eAddr16_Lo = 4,
    eAddr16_Hi = 5,
    eAddr16_Ha = 6,
    eAddr14 = 7,
    eAddr14_BrTaken = 8,
    eAddr14_BrNTaken = 9,
    eRel24 = 10,
    eRel14 = 11,
    eDolphinNop = 201,
    eDolphinSection = 202,
    eDolphinEnd = 203,
    eDolphinMrkref = 204
};

struct SRelocationREL
{
    uint16      Offset;
    ERelocType  Type;
    uint8       SectionID;
    uint32      AddEnd;
};

struct SImportREL
{
    uint32 ModuleID;
    uint32 Offset;
    QVector<SRelocationREL> Relocations;
};

struct SModuleREL
{
    TString             ModuleName;
    uint32              ModuleID;
    uint8               PrologSection;
    uint32              PrologAddress;
    QVector<SSection>   Sections;
    QVector<SImportREL> Imports;
};

struct SRelocationRSO
{
    uint32      Offset;
    uint32      ID; // Internal: Section ID / External: Import Index
    ERelocType  Type;
    uint32      Address;
};

struct SSymbol
{
    TString Name;
    uint32  Address;
    uint32  SectionID;
};

class CDynamicLinker
{
    CByteCodeReader *mpReader;
    uint32 mMemoryPos;

    // Common
    QVector<SSection> mSections;
    uint8 mPrologSection;
    uint32 mPrologAddress;

    // REL
    QMap<uint32,SModuleREL> mModules;

    // RSO
    QVector<SRelocationRSO> mInternals;
    QVector<SRelocationRSO> mExternals;
    QVector<SSymbol> mExports;
    QVector<SSymbol> mImports;
    QVector<QString> mDolSymbols;
    QMap<QString,SSymbol> mDolExports;

public:
    CDynamicLinker(CByteCodeReader *pReader, uint32 StartAddress)
        : mpReader(pReader), mMemoryPos(StartAddress) {}

    void LoadSel(IInputStream& Sel);
    void LinkRELs(const QVector<QString>& kModuleFilenames, QList<SModuleInfo>& ModuleInfoOut);
    void LinkRSOs(const QVector<QString>& kModuleFilenames, QList<SModuleInfo>& ModuleInfoOut);

private:
    uint32 LinkRSO(IInputStream& RSO);
    bool Relocate(uint32 InstructionAddress, uint32 RelocAddress, ERelocType Type);

    uint32 RelSectionAddress(uint32 ModuleID, uint32 SectionID);
    uint32 LoadRSO(IInputStream& RSO);
    void ReadRelocation(SRelocationRSO& Reloc, IInputStream& File);
    void ReadSymbol(SSymbol& Symbol, IInputStream& File, uint32 NamesOffset, bool IsImport);
    uint32 LoadREL(IInputStream& REL);
};

#endif // CDYNAMICLINKER_H
