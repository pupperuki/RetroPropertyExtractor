#include "CEchoesDemoLoader.h"

CEchoesDemoLoader::CEchoesDemoLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : IPropertyLoader(pReader, DolPath, FSTRoot) {}

TString CEchoesDemoLoader::LinkedObjectsFolder()
{
    return mFSTRoot;
}

uint32 CEchoesDemoLoader::ObjectsAddress()
{
    return 0x8038A978;
}

void CEchoesDemoLoader::InitSections()
{
}

void CEchoesDemoLoader::InitGlobals()
{
}

void CEchoesDemoLoader::InitObjects()
{
    mpReader->ExecuteFunction(0x800AC404);
    LoadObjectLoaders();

    mObjects << SObjectInfo { FOURCC('SNFO'), 0x80100DB4, {} }
             << SObjectInfo { FOURCC('TWAM'), 0x80A18334, {} }
             << SObjectInfo { FOURCC('TWBL'), 0x80A184F4, {} }
             << SObjectInfo { FOURCC('TWCB'), 0x80A174A8, {} }
             << SObjectInfo { FOURCC('TWGM'), 0x80A191A0, {} }
             << SObjectInfo { FOURCC('TWGU'), 0x80A1BCD8, {} }
             << SObjectInfo { FOURCC('TWGC'), 0x80A1969C, {} }
             << SObjectInfo { FOURCC('TWPA'), 0x80A19AC4, {} }
             << SObjectInfo { FOURCC('TWPL'), 0x80A17080, {} }
             << SObjectInfo { FOURCC('TWPC'), 0x80A18174, {} }
             << SObjectInfo { FOURCC('TWPG'), 0x80A17874, {} }
             << SObjectInfo { FOURCC('TWPR'), 0x80A19EB4, {} }
             << SObjectInfo { FOURCC('TWSS'), 0x80A187CC, {} }
             << SObjectInfo { FOURCC('TWTG'), 0x80A1A114, {} };
}

bool CEchoesDemoLoader::IsReadFunction(uint32 address)
{
    return ( (address == 0x802B1A94) || (address == 0x802B1AC0) );
}

bool CEchoesDemoLoader::IsSaveRestGPRFunction(uint32 address)
{
    return false;
}

bool CEchoesDemoLoader::IsPropLoader(uint32 address)
{
    return (address == 0x800A6CE4);
}

bool CEchoesDemoLoader::HasInlinedInitializer(uint32 address)
{
    return ( (address == 0x8086CC80) ||
             (address == 0x801EEB6C) ||
             (address == 0x801EED80) ||
             (address == 0x8008B6E4) );
}

bool CEchoesDemoLoader::HasSeparateInitializerFunction(CFourCC ObjType)
{
    return ( (ObjType == FOURCC('ACTR')) ||
             (ObjType == FOURCC('AMIA')) ||
             (ObjType == FOURCC('ATMA')) ||
             (ObjType == FOURCC('BALT')) ||
             (ObjType == FOURCC('BLOG')) ||
             (ObjType == FOURCC('BRZG')) ||
             (ObjType == FOURCC('CAMH')) ||
             (ObjType == FOURCC('CHOG')) ||
             (ObjType == FOURCC('CMDO')) ||
             (ObjType == FOURCC('DTRG')) ||
             (ObjType == FOURCC('DRKS')) ||
             (ObjType == FOURCC('DKTR')) ||
             (ObjType == FOURCC('DBR1')) ||
             (ObjType == FOURCC('DBR2')) ||
             (ObjType == FOURCC('DOOR')) ||
             (ObjType == FOURCC('EFCT')) ||
             (ObjType == FOURCC('EPRT')) ||
             (ObjType == FOURCC('FPRT')) ||
             (ObjType == FOURCC('GBUG')) ||
             (ObjType == FOURCC('GRCH')) ||
             (ObjType == FOURCC('GNTB')) ||
             (ObjType == FOURCC('GNTT')) ||
             (ObjType == FOURCC('INGS')) ||
             (ObjType == FOURCC('ISSW')) ||
             (ObjType == FOURCC('KRAL')) ||
             (ObjType == FOURCC('KROC')) ||
             (ObjType == FOURCC('DLHT')) ||
             (ObjType == FOURCC('LUMI')) ||
             (ObjType == FOURCC('MING')) ||
             (ObjType == FOURCC('MREE')) ||
             (ObjType == FOURCC('MTDA')) ||
             (ObjType == FOURCC('OCTS')) ||
             (ObjType == FOURCC('PARA')) ||
             (ObjType == FOURCC('PCKP')) ||
             (ObjType == FOURCC('PILB')) ||
             (ObjType == FOURCC('PLAT')) ||
             (ObjType == FOURCC('PLAC')) ||
             (ObjType == FOURCC('PLCT')) ||
             (ObjType == FOURCC('SPOR')) ||
             (ObjType == FOURCC('PUFR')) ||
             (ObjType == FOURCC('SNDB')) ||
             (ObjType == FOURCC('WORM')) ||
             (ObjType == FOURCC('SNAK')) ||
             (ObjType == FOURCC('SOND')) ||
             (ObjType == FOURCC('PIRT')) ||
             (ObjType == FOURCC('SPNK')) ||
             (ObjType == FOURCC('SPND')) ||
             (ObjType == FOURCC('SPFN')) ||
             (ObjType == FOURCC('SPTR')) ||
             (ObjType == FOURCC('SPBB')) ||
             (ObjType == FOURCC('SPBN')) ||
             (ObjType == FOURCC('STEM')) ||
             (ObjType == FOURCC('TXPN')) ||
             (ObjType == FOURCC('TRYC')) ||
             (ObjType == FOURCC('FLAR')) ||
             (ObjType == FOURCC('SWRM')) ||
             (ObjType == FOURCC('WATR')) ||
             (ObjType == FOURCC('WLWK')) ||
             (ObjType == FOURCC('WISP')) ||
             (ObjType == FOURCC('SAFE')) ||
             (ObjType == FOURCC('SHRK')) ||
             (ObjType == FOURCC('MNNG')) ||
             (ObjType == FOURCC('COIN')) );
}

bool CEchoesDemoLoader::HasProductionModules()
{
    return true;
}

bool CEchoesDemoLoader::ExecuteReadFunction(uint32 address)
{
    uint32 InputStreamAddress = mpReader->GetRegisterValue(3);
    uint32 StreamPointer;
    mpReader->Load(InputStreamAddress + 8, &StreamPointer, 4);

    // ReadLong
    if (address == 0x802B1A94)
    {
        uint32 Value;
        mpReader->Load(StreamPointer, &Value, 4);
        mpReader->SetRegisterValue(3, Value);
        StreamPointer += 4;
        mpReader->Store(InputStreamAddress + 8, &StreamPointer, 4);
        return true;
    }

    // ReadShort
    else if (address == 0x802B1AC0)
    {
        uint16 Value;
        mpReader->Load(StreamPointer, &Value, 2);
        mpReader->SetRegisterValue(3, Value);
        StreamPointer += 2;
        mpReader->Store(InputStreamAddress + 8, &StreamPointer, 4);
        return true;
    }

    else return false;
}

uint32 CEchoesDemoLoader::GetExternalInitializerAddress(CFourCC ObjType)
{
    switch (ObjType.ToLong())
    {
    case FOURCC('TWAM'): return 0x80A184A0;
    case FOURCC('TWBL'): return 0x80A18750;
    case FOURCC('TWCB'): return 0x80A177A4;
    case FOURCC('TWGM'): return 0x80A19584;
    case FOURCC('TWGU'): return 0x80A1BFC4;
    case FOURCC('TWGC'): return 0x80A19A18;
    case FOURCC('TWPA'): return 0x80A19CB4;
    case FOURCC('TWPL'): return 0x80A173FC;
    case FOURCC('TWPC'): return 0x80A182E0;
    case FOURCC('TWPG'): return 0x80A17C20;
    case FOURCC('TWPM'): return 0x80A1A0A4;
    case FOURCC('TWSS'): return 0x80A18BCC;
    case FOURCC('TWTG'): return 0x80A1B39C;
    default: return 0;
    }
}

EPropertyType CEchoesDemoLoader::PropTypeForLoader(uint32 address)
{
    switch (address)
    {
    case 0x802B1AEC: return EPropertyType::Bool; // CInputStream::ReadBool
    case 0x802B1A94: return EPropertyType::Int; // CInputStream::ReadLong
    case 0x802B1A38: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x802D73A4: return EPropertyType::Color; // CColor::CColor(CInputStream&)
    case 0x8027F2C0: return EPropertyType::Vector3f; // CVector3f::CVector3f(CInputStream&)
    case 0x800ABCF4: return EPropertyType::Character; // LoadTypedefAnimationSet(SLdrAnimationSet&, CInputStream&)
    case 0x800A8470: return EPropertyType::Transform;
    case 0x802B0E2C: return EPropertyType::Invalid; // String constructor
    case 0x802B0890: return EPropertyType::String; // String assign
    case 0x802B0510: return EPropertyType::Invalid; // String dereference
    case 0x802E052C: return EPropertyType::Invalid; // CMayaSpline::CMayaSpline(CInputStream&)
    case 0x80070A8C: return EPropertyType::MayaSpline; // CMayaSpline::__as(const CMayaSpline&)
    case 0x800AACE4: return EPropertyType::LayerSwitch;
    case 0x8020BCC8: return EPropertyType::Invalid; // Array 1
    case 0x801BB8BC: return EPropertyType::Array; // Array 2
    case 0x801BBC58: return EPropertyType::Invalid; // Array 3

    // .rel CMayaSpline::__as copies
    case 0x8080053C: return EPropertyType::MayaSpline;
    case 0x80901CA0: return EPropertyType::MayaSpline;
    case 0x8091FF34: return EPropertyType::MayaSpline;
    case 0x80A1FE7C: return EPropertyType::MayaSpline;

    default: return EPropertyType::Struct;
    }
}

TString CEchoesDemoLoader::GameIdentifier()
{
    return "EchoesDemo";
}
