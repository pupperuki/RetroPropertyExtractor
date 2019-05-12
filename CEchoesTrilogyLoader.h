#ifndef CECHOESTRILOGYLOADER_H
#define CECHOESTRILOGYLOADER_H

#include "IPropertyLoader.h"

class CEchoesTrilogyLoader : public IPropertyLoader
{
public:
    CEchoesTrilogyLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot);
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
    EPropertyType PropTypeForLoader(uint32 address);
    TString GameIdentifier();
};

#endif // CECHOESTRILOGYLOADER_H
