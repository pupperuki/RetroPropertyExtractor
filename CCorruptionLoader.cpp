#include "CCorruptionLoader.h"

CCorruptionLoader::CCorruptionLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : IPropertyLoader(pReader, DolPath, FSTRoot) {}

TString CCorruptionLoader::LinkedObjectsFolder()
{
    return mFSTRoot / "RSO/Rev2 Production/";
}

uint32 CCorruptionLoader::ObjectsAddress()
{
    return 0x805C73D8;
}

void CCorruptionLoader::InitSections()
{
    mpReader->SetSectionAddress(1, 0x80004000);
    mpReader->SetSectionAddress(2, 0x80006EA0);
    mpReader->SetSectionAddress(5, 0x80575F80);
    mpReader->SetSectionAddress(6, 0x8058F180);
    mpReader->SetSectionAddress(7, 0x805C0800);
    mpReader->SetSectionAddress(8, 0x806781C0);
    mpReader->SetSectionAddress(9, 0x8067E9C0);
    mpReader->SetSectionAddress(10, 0x80540928);
    mpReader->SetSectionAddress(11, 0x8067A040);
    mpReader->SetSectionAddress(12, 0x80684380);
}

void CCorruptionLoader::InitGlobals()
{
    // Initialize gkInvalidAssetId
    mpReader->ExecuteFunction(0x803F5CCC);
}

void CCorruptionLoader::InitObjects()
{
    mpReader->ExecuteFunction(0x802DFE94);
    LoadObjectLoaders();

    mObjects << SObjectInfo { FOURCC('SNFO'), 0x801DABC8, {} }
             << SObjectInfo { FOURCC('TWAM'), 0x80DE7570, {} }
             << SObjectInfo { FOURCC('TWBL'), 0x80DE77DC, {} }
             << SObjectInfo { FOURCC('TWCB'), 0x80DE7AFC, {} }
             << SObjectInfo { FOURCC('TWGM'), 0x80DE7F54, {} }
             << SObjectInfo { FOURCC('TWGU'), 0x80DE84CC, {} }
             << SObjectInfo { FOURCC('TWGC'), 0x80DE88B8, {} }
             << SObjectInfo { FOURCC('TWPA'), 0x80DE8C4C, {} }
             << SObjectInfo { FOURCC('TWPL'), 0x80DE8F78, {} }
             << SObjectInfo { FOURCC('TWRC'), 0x80DEF7CC, {} }
             << SObjectInfo { FOURCC('TWAC'), 0x80DEF968, {} }
             << SObjectInfo { FOURCC('TWEC'), 0x80DEFB04, {} }
             << SObjectInfo { FOURCC('TWPG'), 0x80DE93FC, {} }
             << SObjectInfo { FOURCC('TWPR'), 0x80DE9718, {} }
             << SObjectInfo { FOURCC('TWSS'), 0x80DE9A5C, {} }
             << SObjectInfo { FOURCC('TWTG'), 0x80DEA660, {} };
}

bool CCorruptionLoader::IsReadFunction(uint32 address)
{
    return ((address == 0x800B802C) || (address == 0x800B8018) || // Normal ReadLong/ReadShort
            (address == 0x80A4321C) || (address == 0x80A43208) || // RSO_MetroidHatcher.rso ReadLong/ReadShort
            (address == 0x80CFF3A0) || (address == 0x80CFF38C) || // RSO_SeedBoss3.rso ReadLong/ReadShort
            (address == 0x80C508C8) || (address == 0x80C508B4) ); // RSO_ScriptWorldTeleporter.rso ReadLong/ReadShort
}

bool CCorruptionLoader::IsSaveRestGPRFunction(uint32 address)
{
    return (address >= 0x80305A7C && address < 0x80305B14);
}

bool CCorruptionLoader::IsPropLoader(uint32 address)
{
    return ( (address == 0x802D8E84) ||
             (address == 0x80C4E0F0) );
}

bool CCorruptionLoader::HasInlinedInitializer(uint32 address)
{
    return ( (address == 0x801EE978) || // CameraHint 0x00A7C38D
             (address == 0x801ECA68) || // CameraHint 0x4BE3494B
             (address == 0x801EEA24) || // CameraHint 0xFC126AD1
             (address == 0x801EC96C) || // CameraHint 0xEBC3E775
             (address == 0x801EF690) || // CameraHint 0x97A93F8F
             (address == 0x801EF384) || // CameraHint 0x764827D4
             (address == 0x801F655C) ||
             (address == 0x80C5623C) ||
             (address == 0x80C50DC8) ||
             (address == 0x8008DEE4) ); // OPAA 0x0FF44A68
}

bool CCorruptionLoader::HasSeparateInitializerFunction(CFourCC ObjType)
{
    return ( (ObjType == FOURCC('CAMH')) || // CameraHint
             (ObjType == FOURCC('MHAT')) || // MetroidHatcher
             (ObjType == FOURCC('BOS3')) || // SeedBoss3
             (ObjType == FOURCC('SHCP')) || // ShipCommandPath
             (ObjType == FOURCC('TEL1')) ); // WorldTeleporter
}

uint32 CCorruptionLoader::GetExternalInitializerAddress(CFourCC ObjType)
{
    switch (ObjType.ToLong())
    {
    case FOURCC('TWAM'): return 0x80DE74A4;
    case FOURCC('TWBL'): return 0x80DE7670;
    case FOURCC('TWCB'): return 0x80DE79F8;
    case FOURCC('TWGM'): return 0x80DE7DA8;
    case FOURCC('TWGU'): return 0x80DE8360;
    case FOURCC('TWGC'): return 0x80DE86E8;
    case FOURCC('TWPA'): return 0x80DE8B88;
    case FOURCC('TWPL'): return 0x80DE8DB0;
    case FOURCC('TWRC'): return 0x80DEF714;
    case FOURCC('TWAC'): return 0x80DEF8B0;
    case FOURCC('TWEC'): return 0x80DEFA4C;
    case FOURCC('TWPG'): return 0x80DE9290;
    case FOURCC('TWPR'): return 0x80DE9618;
    case FOURCC('TWSS'): return 0x80DE986C;
    case FOURCC('TWTG'): return 0x80DE9DD4;
    default: return 0;
    }
}

EPropertyType CCorruptionLoader::PropTypeForLoader(uint32 address)
{
    switch (address)
    {
    case 0x800B8018: return EPropertyType::Short; // CInputStream::ReadShort
    case 0x800B802C: return EPropertyType::Int; // CInputStream::ReadLong
    case 0x800C1D14: return EPropertyType::Int;
    case 0x800B8000: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x803EDB88: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x802D68D8: return EPropertyType::Enum; // LdrFindEnumEntry
    case 0x802D69D4: return EPropertyType::Enum; // LdrFindEnumEntryLinear
    case 0x803E71E0: return EPropertyType::Color; // CColor::CColor(CInputStream&)
    case 0x803E23B0: return EPropertyType::Vector3f; // CVector3f::CVector3f(CInputStream&)
    case 0x803F5C14: return EPropertyType::Asset; // CAssetID::CAssetID(CInputStream&)
    case 0x802DC4E8: return EPropertyType::Transform;
    case 0x8037E63C: return EPropertyType::Invalid; // CMayaSpline::CMayaSpline(CInputStream&)
    case 0x8037EA2C: return EPropertyType::MayaSpline; // CMayaSpline::__as(const CMayaSpline&)
    case 0x803825E0: return EPropertyType::Invalid; // CMemory::Free(void*)
    case 0x8037EA8C: return EPropertyType::MayaSpline; // CMayaSpline::AssignFromStream(CInputStream&)
    case 0x802D6DF4: return EPropertyType::Character; // LoadTypedefCharacterAnimationSet(SLdrCharacterAnimationSet&, CInputStream&)
    case 0x803FC200: return EPropertyType::Invalid; // String constructor
    case 0x803FD4E8: return EPropertyType::String; // String assign
    case 0x803FEFF0: return EPropertyType::Invalid; // String dereference
    case 0x803756B8: return EPropertyType::LayerID;
    case 0x802D6ED4: return EPropertyType::LayerSwitch;
    case 0x804B718C: return EPropertyType::Array;
    case 0x80C508C8: return EPropertyType::Int; // CInputStream::ReadLong in RSO_ScriptWorldTeleporter.rso
    case 0x80C508FC: return EPropertyType::Float; // CInputStream::ReadFloat in RSO_ScriptWorldTeleporter.rso
    case 0x80C4D3E4: return EPropertyType::Invalid; // Function before string assign in RSO_ScriptWorldTeleporter.rso
    case 0x80C4FAD4: return EPropertyType::String; // RSO_ScriptWorldTeleporter.rso string assign
    case 0x80C4D3E8: return EPropertyType::Invalid; // RSO_ScriptWorldTeleporter.rso string dereference
    case 0x80C508DC: return EPropertyType::Bool; // RSO_ScriptWorldTeleporter.rso Read Bool proxy
    default: return EPropertyType::Struct;
    }
}

TString CCorruptionLoader::GameIdentifier()
{
    return "Corruption";
}
