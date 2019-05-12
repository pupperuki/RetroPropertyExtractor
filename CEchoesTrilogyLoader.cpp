#include "CEchoesTrilogyLoader.h"

CEchoesTrilogyLoader::CEchoesTrilogyLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : IPropertyLoader(pReader, DolPath, FSTRoot) {}

TString CEchoesTrilogyLoader::LinkedObjectsFolder()
{
    return mFSTRoot / "RSO/Production/";
}

uint32 CEchoesTrilogyLoader::ObjectsAddress()
{
    return 0x80507920;
}

void CEchoesTrilogyLoader::InitSections()
{
    mpReader->SetSectionAddress(1, 0x80004000);
    mpReader->SetSectionAddress(2, 0x80006500);
    mpReader->SetSectionAddress(5, 0x804A0400);
    mpReader->SetSectionAddress(6, 0x804B3160);
    mpReader->SetSectionAddress(7, 0x804E0AC0);
    mpReader->SetSectionAddress(8, 0x805C9660);
    mpReader->SetSectionAddress(9, 0x805CC280);
    mpReader->SetSectionAddress(11, 0x805CAC20);
    mpReader->SetSectionAddress(12, 0x805D16A0);
}

void CEchoesTrilogyLoader::InitGlobals()
{
}

void CEchoesTrilogyLoader::InitObjects()
{
    mpReader->ExecuteFunction(0x8029C2E0);
    LoadObjectLoaders();
}

bool CEchoesTrilogyLoader::IsReadFunction(uint32 address)
{
    return ( (address == 0x800B9A34) || (address == 0x800B9A48) || // Regular ReadLong/ReadShort
             (address == 0x809C8AA8) || (address == 0x809C8A94) ); // RSO_IngBoostBallGuardian.rso ReadLong/ReadShort
}

bool CEchoesTrilogyLoader::IsSaveRestGPRFunction(uint32 address)
{
    return (address >= 0x802B8388 && address < 0x802B8420);
}

bool CEchoesTrilogyLoader::IsPropLoader(uint32 address)
{
    return ( (address == 0x803D1764) ||
             (address == 0x803D171C) ||
             (address == 0x80297EB0) ||
             (address == 0x800B9A34) ||
             (address == 0x800C1FC0) ||
             (address == 0x800C1FEC) );
}

bool CEchoesTrilogyLoader::HasInlinedInitializer(uint32 address)
{
    return ( (address == 0x80A5133C) ||
             (address == 0x809C8D44) ||
             (address == 0x8084957C) ||
             (address == 0x80849870) ||
             (address == 0x801DE44C) ||
             (address == 0x801ECD14) ||
             (address == 0x801ECE30) ||
             (address == 0x801ECAF4) ||
             (address == 0x80826B6C) ||
             (address == 0x8083525C) ||
             (address == 0x809C8ABC) ||
             (address == 0x80A5100C) ||
             (address == 0x8029430C) ||
             (address == 0x80B4DDC0) ||
             (address == 0x80B4DA20) ||
             (address == 0x80B86688) ||
             (address == 0x80227238) ||
             (address == 0x8022BF1C) );
}

bool CEchoesTrilogyLoader::HasSeparateInitializerFunction(CFourCC ObjType)
{
    return ( (ObjType == FOURCC('IBBG')) || // IngBoostBallGuardian
             (ObjType == FOURCC('DLHT')) || // DynamicLight
             (ObjType == FOURCC('PLAT')) ); // Platform
}

EPropertyType CEchoesTrilogyLoader::PropTypeForLoader(uint32 address)
{
    switch (address)
    {
    case 0x800B99F8: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x80328750: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x80376458: return EPropertyType::Float; // CInputStream::ReadFloat
    case 0x8036F7C8: return EPropertyType::Color; // CColor::CColor(CInputStream&)
    case 0x8031183C: return EPropertyType::Invalid; // CMayaSpline::CMayaSpline(CInputStream&)
    case 0x80311BD8: return EPropertyType::MayaSpline; // CMayaSpline::__as(const CMayaSpline&)
    case 0x80369944: return EPropertyType::Vector3f; // CVector3f::CVector3f(CInputStream&)
    case 0x80293CC4: return EPropertyType::Character; // LoadTypedefAnimationSet(CInputStream&)
    case 0x8038793C: return EPropertyType::Invalid; // String constructor
    case 0x80388C24: return EPropertyType::String; // String assign
    case 0x8038A72C: return EPropertyType::Invalid; // String dereference
    case 0x80296620: return EPropertyType::Transform;
    case 0x801F3EE8: return EPropertyType::LayerSwitch;
    case 0x803CFBEC: return EPropertyType::Invalid; // Array 1
    case 0x80208794: return EPropertyType::Array; // Array 2
    case 0x80207570: return EPropertyType::Invalid; // Array 3
    default: return EPropertyType::Struct;
    }
}

TString CEchoesTrilogyLoader::GameIdentifier()
{
    return "EchoesTrilogy";
}
