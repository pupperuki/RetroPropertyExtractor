#ifndef IPROPERTYLOADER_H
#define IPROPERTYLOADER_H

#include "CByteCodeReader.h"
#include "SProperty.h"
#include <Common/CFourCC.h>
#include <QMap>
#include <QVector>

struct SObjectInfo
{
    CFourCC Type;
    uint32 LoaderAddress;
    SStruct BaseStruct;
};

class IPropertyLoader
{
protected:
    CByteCodeReader *mpReader;
    TString mDolPath;
    TString mFSTRoot;

    // Properties
    QVector<SObjectInfo> mObjects;

    struct SLinkedStructInfo
    {
        uint32 InitializerAddress;
        QMap<uint32,SLinkedStructInfo> SubStructs; // Key: Data Address / Value: Sub Struct
        SLinkedStructInfo *pParent;

        SLinkedStructInfo()
            : InitializerAddress(0), pParent(nullptr) {}
    };
    QMap<uint32,SLinkedStructInfo> mLinkMap; // Key: Object ID / Value: Linked Struct Info

    // Temp variables
    uint32 mInputStreamAddress;
    QMap<uint32,SReaderState> mPropertyStates; // Key: Loader Address / Value: Property Loader Reader State

    static const uint32 mskDummyPropID = 0x1234ABCD;

public:
    IPropertyLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot);
    virtual ~IPropertyLoader();
    virtual TString LinkedObjectsFolder() = 0;
    virtual uint32 ObjectsAddress() = 0;
    virtual void InitSections() = 0;
    virtual void InitGlobals() = 0;
    virtual void InitObjects() = 0;
    virtual bool IsReadFunction(uint32 address) = 0;
    virtual bool IsSaveRestGPRFunction(uint32 address) = 0;
    virtual bool IsPropLoader(uint32 address) = 0;
    virtual bool HasInlinedInitializer(uint32 address) = 0;
    virtual bool HasSeparateInitializerFunction(CFourCC ObjType) = 0;
    virtual uint32 GetExternalInitializerAddress(CFourCC ObjType) { return 0; }
    virtual bool HasProductionModules() { return false; }
    virtual bool ShouldGetErrorStrings() { return false; }
    virtual bool ExecuteReadFunction(uint32 /*address*/) { return false; }
    virtual EPropertyType PropTypeForLoader(uint32 address) = 0;
    virtual TString GameIdentifier() = 0;
    void LoadObjectLoaders();
    void ParseObjectLoaders();
    void ParseExternalInitializer(SLinkedStructInfo *pLinkInfo, uint32 Address);
    void InitStructLoader(uint32 Address, bool IsIndependent);
    void ParseStructLoader(SLinkedStructInfo *pLinkInfo, uint32 Address, bool IsIndependent);
    void ParseStructIDs(SStruct *pStruct, SLinkedStructInfo *pLinkInfo);
    void ParseStructTypes(SStruct *pStruct, SLinkedStructInfo *pLinkInfo);
    void CalculateStructInitializerOffsets(SStruct *pStruct);
    void LoadIndependentStruct(SStruct *pStruct);
    SProperty* ReadInlineEnum(SStruct *pStruct, SProperty *pProp);

    uint32 NumObjects();
    SObjectInfo* Object(uint32 index);

    SLinkedStructInfo* LinkStruct(SLinkedStructInfo *pLinkInfo, uint32 DataAddress, uint32 InitializerAddress);
    void ResetInputStream();
    bool IsInputStreamRegister(uint32 RegID);

    void SortProperties(SStruct *pStruct);
    void DebugPrintProperties(SStruct *pStruct, uint32 Depth = 0);
    SProperty* MakeProperty(SProperty *pBase, SStruct *pStruct, EPropertyType Type);
};

#endif // IPROPERTYLOADER_H
