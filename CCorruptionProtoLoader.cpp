#include "CCorruptionProtoLoader.h"

CCorruptionProtoLoader::CCorruptionProtoLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : IPropertyLoader(pReader, DolPath, FSTRoot) {}

TString CCorruptionProtoLoader::LinkedObjectsFolder()
{
    return "";
}

uint32 CCorruptionProtoLoader::ObjectsAddress()
{
    return 0x80703448;
}

void CCorruptionProtoLoader::InitSections()
{
}

void CCorruptionProtoLoader::InitGlobals()
{
}

void CCorruptionProtoLoader::InitObjects()
{
    mpReader->ExecuteFunction(0x803649A0);
    LoadObjectLoaders();

    mObjects << SObjectInfo { FOURCC('SNFO'), 0x8011C6DC, {} }
             << SObjectInfo { FOURCC('TWAM'), 0x80246FB4, {} }
             << SObjectInfo { FOURCC('TWBL'), 0x80246C18, {} }
             << SObjectInfo { FOURCC('TWCB'), 0x80246880, {} }
             << SObjectInfo { FOURCC('TWGM'), 0x802461C8, {} }
             << SObjectInfo { FOURCC('TWGU'), 0x80245ED4, {} }
             << SObjectInfo { FOURCC('TWGC'), 0x80245A58, {} }
             << SObjectInfo { FOURCC('TWPA'), 0x8024580C, {} }
             << SObjectInfo { FOURCC('TWPL'), 0x802452B4, {} }
             << SObjectInfo { FOURCC('TWPC'), 0x8023ECF8, {} }
             << SObjectInfo { FOURCC('TWPG'), 0x80244F04, {} }
             << SObjectInfo { FOURCC('TWPR'), 0x80244C8C, {} }
             << SObjectInfo { FOURCC('TWSS'), 0x80244730, {} }
             << SObjectInfo { FOURCC('TWTG'), 0x80242D98, {} };
}

bool CCorruptionProtoLoader::IsReadFunction(uint32 address)
{
    return false;
}

bool CCorruptionProtoLoader::IsSaveRestGPRFunction(uint32 address)
{
    return false;
}

bool CCorruptionProtoLoader::IsPropLoader(uint32 address)
{
    return (address == 0x801D33C8);
}

bool CCorruptionProtoLoader::HasInlinedInitializer(uint32 address)
{
    return ( (address == 0x800BD5A4) ||
             (address == 0x800B9E5C) ||
             (address == 0x800BD4E4) ||
             (address == 0x800BAE50) ||
             (address == 0x800BABE8) ||
             (address == 0x800B9F1C) ||
             (address == 0x8020D5E4) ||
             (address == 0x8005ACB8) );
}

bool CCorruptionProtoLoader::HasSeparateInitializerFunction(CFourCC ObjType)
{
    return ( (ObjType == FOURCC('ACTR')) ||
             (ObjType == FOURCC('AMIA')) ||
             (ObjType == FOURCC('ATMA')) ||
             (ObjType == FOURCC('BSKR')) ||
             (ObjType == FOURCC('CAMH')) ||
             (ObjType == FOURCC('CINE')) ||
             (ObjType == FOURCC('DTRG')) ||
             (ObjType == FOURCC('DBAR')) ||
             (ObjType == FOURCC('DOOR')) ||
             (ObjType == FOURCC('DLHT')) ||
             (ObjType == FOURCC('EFCT')) ||
             (ObjType == FOURCC('FISH')) ||
             (ObjType == FOURCC('PCKP')) ||
             (ObjType == FOURCC('PLAT')) ||
             (ObjType == FOURCC('PLAC')) ||
             (ObjType == FOURCC('PLCT')) ||
             (ObjType == FOURCC('SHCI')) ||
             (ObjType == FOURCC('SHCP')) ||
             (ObjType == FOURCC('SOND')) ||
             (ObjType == FOURCC('PIRT')) ||
             (ObjType == FOURCC('SPFN')) ||
             (ObjType == FOURCC('STEM')) ||
             (ObjType == FOURCC('SUBT')) ||
             (ObjType == FOURCC('TXPN')) ||
             (ObjType == FOURCC('FLAR')) );
}

bool CCorruptionProtoLoader::ShouldGetErrorStrings()
{
    return true;
}

uint32 CCorruptionProtoLoader::GetExternalInitializerAddress(CFourCC ObjType)
{
    switch (ObjType.ToLong())
    {
    case FOURCC('TWAM'): return 0x80247140;
    case FOURCC('TWBL'): return 0x80246F24;
    case FOURCC('TWCB'): return 0x80246B9C;
    case FOURCC('TWGM'): return 0x80246770;
    case FOURCC('TWGU'): return 0x80246150;
    case FOURCC('TWGC'): return 0x80245E24;
    case FOURCC('TWPA'): return 0x80245A1C;
    case FOURCC('TWPL'): return 0x8024574C;
    case FOURCC('TWPC'): return 0x8023EE84;
    case FOURCC('TWPG'): return 0x80245210;
    case FOURCC('TWPM'): return 0x80244E9C;
    case FOURCC('TWSS'): return 0x80244B50;
    case FOURCC('TWTG'): return 0x80244088;
    default: return 0;
    }
}

EPropertyType CCorruptionProtoLoader::PropTypeForLoader(uint32 address)
{
    switch (address)
    {
    case 0x80556990: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x80508608: return EPropertyType::Asset; // CAssetID::CAssetID(CInputStream&)
    case 0x801D5964: return EPropertyType::Character; // LoadTypedefCharacterAnimationSet(SLdrAnimationSet&, CInputStream&)
    case 0x804CC8F4: return EPropertyType::Vector3f; // CVector3f::CVector3f(CInputStream&)
    case 0x80535C70: return EPropertyType::Color; // CColor::CColor(CInputStream&)
    case 0x801CA1D4: return EPropertyType::Transform;
    case 0x80504F2C: return EPropertyType::Invalid; // String constructor
    case 0x8050493C: return EPropertyType::String; // String assign
    case 0x80504438: return EPropertyType::Invalid; // String dereference
    case 0x80547118: return EPropertyType::Invalid; // CMayaSpline::CMayaSpline(CInputStream&)
    case 0x800689C0: return EPropertyType::MayaSpline; // CMayaSpline::__as(const CMayaSpline&)
    case 0x801D58B4: return EPropertyType::LayerSwitch;
    case 0x805AE9B0: return EPropertyType::Invalid; // Array 1
    case 0x8020E3AC: return EPropertyType::Array; // Array 2
    case 0x8020E894: return EPropertyType::Invalid; // Array 3
    default: return EPropertyType::Struct;
    }
}

TString CCorruptionProtoLoader::GameIdentifier()
{
    return "CorruptionProto";
}
