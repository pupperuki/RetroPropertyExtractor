#include "CDKLoader.h"
#include "Masks.h"
#include <iostream>

CDKLoader::CDKLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : IPropertyLoader(pReader, DolPath, FSTRoot) {}

TString CDKLoader::LinkedObjectsFolder()
{
    return mFSTRoot / "/RSO/wii_production/";
}

uint32 CDKLoader::ObjectsAddress()
{
    return mpReader->GetBssAddress() + 0xAA8;
}

void CDKLoader::InitSections()
{
    mpReader->SetSectionAddress(1, 0x80004000);
    mpReader->SetSectionAddress(2, 0x800068E0);
    mpReader->SetSectionAddress(5, 0x80544580);
    mpReader->SetSectionAddress(6, 0x805650E0);
    mpReader->SetSectionAddress(7, 0x805A7F80);
    mpReader->SetSectionAddress(8, 0x8061E120);
    mpReader->SetSectionAddress(9, 0x8061FC20);
    mpReader->SetSectionAddress(11, 0x8061EA80);
    mpReader->SetSectionAddress(12, 0x80624A80);
}

void CDKLoader::InitGlobals()
{
    // Initialize gkInvalidAssetId
    mpReader->ExecuteFunction(0x80428E90);
    mpReader->ExecuteFunction(0x800F2810);
}

void CDKLoader::InitObjects()
{
    mpReader->ExecuteFunction(0x800EF1F0);
    LoadObjectLoaders();

    mObjects << SObjectInfo { FOURCC('TWCT'), 0x808E30C4, {} }
             << SObjectInfo { FOURCC('TWGT'), 0x808E3E84, {} };
}

bool CDKLoader::IsReadFunction(uint32 address)
{
    return ((address == 0x800BCF90) || (address == 0x800BCF50));
}

bool CDKLoader::IsSaveRestGPRFunction(uint32 address)
{
    return ((address >= 0x804768E0) && (address < 0x80476978));
}

bool CDKLoader::IsPropLoader(uint32 address)
{
    return (address == 0x8019D770);
}

bool CDKLoader::HasInlinedInitializer(uint32 address)
{
    return ( (address == 0x800FAF00) || // Acoustics 0xA383E10B
             (address == 0x800FB110) || // Acoustics 0x6530E9CF
             (address == 0x800FB220) || // Acoustics 0x3BC9DA35
             (address == 0x80119FC0) || // CameraHint 0x97A93F8F
             (address == 0x80117630) || // CameraHint 0x4BE3494B
             (address == 0x80119120) || // CameraHint 0x00A7C38D
             (address == 0x801191D0) || // CameraHint 0xFC126AD1
             (address == 0x80117540) || // CameraHint 0xEBC3E775
             (address == 0x80117420) || // CameraHint 0xEBC3E775:0xB40A41B8
             (address == 0x8013BAF0) || // GeneratedObjectDeleter 0x2013052B
             (address == 0x800984D0) || // OPAA 0x0FF44A68
             (address == 0x80198EB0) ); // Tutorial 0x76AA6D55
}

bool CDKLoader::HasSeparateInitializerFunction(CFourCC ObjType)
{
    return (ObjType == FOURCC('CAMH')); // CameraHint
}

uint32 CDKLoader::GetExternalInitializerAddress(CFourCC ObjType)
{
    switch (ObjType.ToLong())
    {
    case FOURCC('TWCT'): return 0x808E2FD4;
    case FOURCC('TWGT'): return 0x808E3DC4;
    default: return 0;
    }
}

EPropertyType CDKLoader::PropTypeForLoader(uint32 address)
{
    switch (address)
    {
    case 0x804434E0: return EPropertyType::Vector3f;
    case 0x80426F50: return EPropertyType::Float;
    case 0x80117890: return EPropertyType::Float;
    case 0x8011D12C: return EPropertyType::Int;
    case 0x800BCF90: return EPropertyType::Int;
    case 0x800BCE40: return EPropertyType::Int;
    case 0x80428DD0: return EPropertyType::Asset;
    case 0x80417950: return EPropertyType::Invalid; // String constructor
    case 0x80418CC0: return EPropertyType::String; // String assign
    case 0x8041A890: return EPropertyType::Invalid; // String dereference
    case 0x803AD920: return EPropertyType::Color;
    case 0x80436950: return EPropertyType::MayaSpline;
    case 0x8019CA80: return EPropertyType::Character;
    case 0x8019C6C0: return EPropertyType::Enum;
    case 0x8019C5B0: return EPropertyType::Enum;
    case 0x8019FC60: return EPropertyType::Transform;
    case 0x8019CC30: return EPropertyType::LayerSwitch;
    case 0x803BE270: return EPropertyType::LayerID;
    case 0x80329800: return EPropertyType::Array;
    default: return EPropertyType::Struct;
    }
}

TString CDKLoader::GameIdentifier()
{
    return "DKCReturns";
}
