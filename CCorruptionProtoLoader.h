#ifndef CCORRUPTIONPROTOLOADER_H
#define CCORRUPTIONPROTOLOADER_H

#include "IPropertyLoader.h"

class CCorruptionProtoLoader : public IPropertyLoader
{
public:
    CCorruptionProtoLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot);
    TString LinkedObjectsFolder();
    uint32 ObjectsAddress();
    void InitSections();
    void InitGlobals();
    void InitObjects();
    bool IsReadFunction(uint32 address);
    bool IsSaveRestGPRFunction(uint32 address);
    bool IsPropLoader(uint32 address);
    bool HasInlinedInitializer(uint32 address);
    bool HasSeparateInitializerFunction(CFourCC ObjType);
    bool ShouldGetErrorStrings();
    uint32 GetExternalInitializerAddress(CFourCC ObjType);
    EPropertyType PropTypeForLoader(uint32 address);
    TString GameIdentifier();
};

#endif // CCORRUPTIONPROTOLOADER_H
