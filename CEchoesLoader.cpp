#include "CEchoesLoader.h"

CEchoesLoader::CEchoesLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : IPropertyLoader(pReader, DolPath, FSTRoot) {}

TString CEchoesLoader::LinkedObjectsFolder()
{
    return mFSTRoot / "RelProd/";
}

uint32 CEchoesLoader::ObjectsAddress()
{
    return 0x803DE138;
}

void CEchoesLoader::InitSections()
{
}

void CEchoesLoader::InitGlobals()
{
}

void CEchoesLoader::InitObjects()
{
    mpReader->ExecuteFunction(0x80242894);
    LoadObjectLoaders();

    mObjects << SObjectInfo { FOURCC('SNFO'), 0x80111564, {} }
             << SObjectInfo { FOURCC('TWAM'), 0x80B4E988, {} }
             << SObjectInfo { FOURCC('TWBL'), 0x80B4EB50, {} }
             << SObjectInfo { FOURCC('TWCB'), 0x80B4DADC, {} }
             << SObjectInfo { FOURCC('TWGM'), 0x80B4F814, {} }
             << SObjectInfo { FOURCC('TWGU'), 0x80B526EC, {} }
             << SObjectInfo { FOURCC('TWGC'), 0x80B50034, {} }
             << SObjectInfo { FOURCC('TWPA'), 0x80B5049C, {} }
             << SObjectInfo { FOURCC('TWPL'), 0x80B4D6AC, {} }
             << SObjectInfo { FOURCC('TWPC'), 0x80B4E7C0, {} }
             << SObjectInfo { FOURCC('TWPG'), 0x80B4DEB0, {} }
             << SObjectInfo { FOURCC('TWPR'), 0x80B5089C, {} }
             << SObjectInfo { FOURCC('TWSS'), 0x80B4EE30, {} }
             << SObjectInfo { FOURCC('TWTG'), 0x80B50B04, {} };
}

bool CEchoesLoader::IsReadFunction(uint32 address)
{
    return false;
}

bool CEchoesLoader::IsSaveRestGPRFunction(uint32 address)
{
    return false;
}

bool CEchoesLoader::IsPropLoader(uint32 address)
{
    return (address == 0x8023E0C4);
}

bool CEchoesLoader::HasInlinedInitializer(uint32 address)
{
    return ( (address == 0x8022CC4C) ||
             (address == 0x802206A4) ||
             (address == 0x802208F0) ||
             (address == 0x808180E8) ||
             (address == 0x80828CF4) ||
             (address == 0x8096DB00) ||
             (address == 0x8009FFFC) ||
             (address == 0x80A8CFD0) ||
             (address == 0x80147584) ||
             (address == 0x800D78A0) );
}

bool CEchoesLoader::HasSeparateInitializerFunction(CFourCC ObjType)
{
    return ( (ObjType == FOURCC('ACTR')) ||
             (ObjType == FOURCC('AMIA')) ||
             (ObjType == FOURCC('ATMA')) ||
             (ObjType == FOURCC('ATMB')) ||
             (ObjType == FOURCC('BSWM')) ||
             (ObjType == FOURCC('BALT')) ||
             (ObjType == FOURCC('BLOG')) ||
             (ObjType == FOURCC('BRZG')) ||
             (ObjType == FOURCC('CAMH')) ||
             (ObjType == FOURCC('CHOG')) ||
             (ObjType == FOURCC('COIN')) ||
             (ObjType == FOURCC('CMDO')) ||
             (ObjType == FOURCC('CTLH')) ||
             (ObjType == FOURCC('DTRG')) ||
             (ObjType == FOURCC('DRKS')) ||
             (ObjType == FOURCC('DKTR')) ||
             (ObjType == FOURCC('DBR1')) ||
             (ObjType == FOURCC('DBR2')) ||
             (ObjType == FOURCC('DBAR')) ||
             (ObjType == FOURCC('DOOR')) ||
             (ObjType == FOURCC('EFCT')) ||
             (ObjType == FOURCC('EPRT')) ||
             (ObjType == FOURCC('EYEB')) ||
             (ObjType == FOURCC('FPRT')) ||
             (ObjType == FOURCC('GBUG')) ||
             (ObjType == FOURCC('GRCH')) ||
             (ObjType == FOURCC('GNTB')) ||
             (ObjType == FOURCC('GNTT')) ||
             (ObjType == FOURCC('INGS')) ||
             (ObjType == FOURCC('IBSM')) ||
             (ObjType == FOURCC('IBBG')) ||
             (ObjType == FOURCC('ISSW')) ||
             (ObjType == FOURCC('KRAL')) ||
             (ObjType == FOURCC('KROC')) ||
             (ObjType == FOURCC('DLHT')) ||
             (ObjType == FOURCC('LUMI')) ||
             (ObjType == FOURCC('MING')) ||
             (ObjType == FOURCC('MREE')) ||
             (ObjType == FOURCC('MTDA')) ||
             (ObjType == FOURCC('MNNG')) ||
             (ObjType == FOURCC('OCTS')) ||
             (ObjType == FOURCC('PARA')) ||
             (ObjType == FOURCC('PCKP')) ||
             (ObjType == FOURCC('PILB')) ||
             (ObjType == FOURCC('PLAT')) ||
             (ObjType == FOURCC('PSSM')) ||
             (ObjType == FOURCC('PLAC')) ||
             (ObjType == FOURCC('PLCT')) ||
             (ObjType == FOURCC('PLRT')) ||
             (ObjType == FOURCC('SPOR')) ||
             (ObjType == FOURCC('PUFR')) ||
             (ObjType == FOURCC('SAFE')) ||
             (ObjType == FOURCC('SFZC')) ||
             (ObjType == FOURCC('WORM')) ||
             (ObjType == FOURCC('SHRK')) ||
             (ObjType == FOURCC('SNAK')) ||
             (ObjType == FOURCC('SOND')) ||
             (ObjType == FOURCC('PIRT')) ||
             (ObjType == FOURCC('SPNK')) ||
             (ObjType == FOURCC('SPND')) ||
             (ObjType == FOURCC('SPTR')) ||
             (ObjType == FOURCC('SPBB')) ||
             (ObjType == FOURCC('SPBN')) ||
             (ObjType == FOURCC('STEM')) ||
             (ObjType == FOURCC('SBS2')) ||
             (ObjType == FOURCC('TXPN')) ||
             (ObjType == FOURCC('TRYC')) ||
             (ObjType == FOURCC('FLAR')) ||
             (ObjType == FOURCC('WATR')) ||
             (ObjType == FOURCC('WLWK')) ||
             (ObjType == FOURCC('WISP')) );
}

uint32 CEchoesLoader::GetExternalInitializerAddress(CFourCC ObjType)
{
    switch (ObjType.ToLong())
    {
    case FOURCC('TWAM'): return 0x80B4EAFC;
    case FOURCC('TWBL'): return 0x80B4EDB4;
    case FOURCC('TWCB'): return 0x80B4DDE0;
    case FOURCC('TWGM'): return 0x80B4FEE4;
    case FOURCC('TWGU'): return 0x80B529B0;
    case FOURCC('TWGC'): return 0x80B503E8;
    case FOURCC('TWPA'): return 0x80B50694;
    case FOURCC('TWPL'): return 0x80B4DA30;
    case FOURCC('TWPC'): return 0x80B4E934;
    case FOURCC('TWPG'): return 0x80B4E264;
    case FOURCC('TWPM'): return 0x80B50A94;
    case FOURCC('TWSS'): return 0x80B4F238;
    case FOURCC('TWTG'): return 0x80B51DB0;
    default: return 0;
    }
}

EPropertyType CEchoesLoader::PropTypeForLoader(uint32 address)
{
    switch (address)
    {
    case 0x80342F90: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x8032085C: return EPropertyType::Color; // CColor::CColor(CInputStream&)
    case 0x80242268: return EPropertyType::Character; // LoadTypedefAnimationSet(SLdrAnimationSet&, CInputStream&)
    case 0x802CB058: return EPropertyType::Vector3f; // CVector3f::CVector3f(CInputStream&)
    case 0x802FF2CC: return EPropertyType::Invalid; // String constructor
    case 0x802FED30: return EPropertyType::String; // String assign
    case 0x802FE9B8: return EPropertyType::Invalid; // String dereference
    case 0x8023F8CC: return EPropertyType::Transform;
    case 0x80329C7C: return EPropertyType::MayaSpline; // CMayaSpline::CMayaSpline(CInputStream&)
    case 0x80080DB8: return EPropertyType::MayaSpline; // CMayaSpline::__as(const CMayaSpline&)
    case 0x808004EC: return EPropertyType::MayaSpline;
    case 0x80A63F74: return EPropertyType::MayaSpline;
    case 0x809B7E8C: return EPropertyType::MayaSpline;
    case 0x80A91AF0: return EPropertyType::MayaSpline;
    case 0x8096D968: return EPropertyType::MayaSpline;
    case 0x8022F954: return EPropertyType::LayerSwitch;
    case 0x80255198: return EPropertyType::Invalid; // Array 1
    case 0x801E15EC: return EPropertyType::Array; // Array 2
    case 0x801E1A20: return EPropertyType::Invalid; // Array 3
    default: return EPropertyType::Struct;
    }
}

TString CEchoesLoader::GameIdentifier()
{
    return "Echoes";
}
