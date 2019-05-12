#ifndef CDKLOADER_H
#define CDKLOADER_H

#include "IPropertyLoader.h"

class CDKLoader : public IPropertyLoader
{
public:
    CDKLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot);
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
    uint32 GetExternalInitializerAddress(CFourCC ObjType);
    EPropertyType PropTypeForLoader(uint32 address);
    TString GameIdentifier();
};

#endif // CDKLOADER_H
