#ifndef CTEMPLATEWRITER_H
#define CTEMPLATEWRITER_H

#include "IPropertyLoader.h"
#include <tinyxml2.h>
#include <QMap>
#include <QSet>
#include <QString>

class CTemplateWriter
{
    IPropertyLoader *mpLoader;

    QString mOutputDir;
    QString mStructsDir;
    QString mEnumsDir;
    QString mScriptDir;
    uint32 mStructNum;
    uint32 mEnumNum;
    bool mNoDefaults;

    struct SUniqueStruct
    {
        SStruct Struct;
        uint32 NumReferences;
        QString TemplatePath;
        QString ProtoName;
        bool NoDefaults;
    };
    QMap<uint32,SUniqueStruct> mUniqueStructs;

    struct SUniqueEnum
    {
        SEnum Enum;
        uint32 NumReferences;
        QString TemplatePath;
        QString ProtoName;
    };
    QMap<uint64,SUniqueEnum> mUniqueEnums;

    QMap<QString,QString> mNames;
    QMap<uint32,TString> mExtensions;
    QMap<uint32,QString> mUniqueProperties;
    QSet<uint32> mParsedStructs;

public:
    CTemplateWriter(IPropertyLoader *pPropLoader)
        : mpLoader(pPropLoader) {}

    void MakeTemplates();

private:
    bool ShouldBeExternal(SUniqueStruct *pStruct);
    void FindUniqueProperties(SStruct *pParentStruct);
    void WriteScriptTemplate(SObjectInfo *pObj);
    void WriteStructTemplate(SStruct *pStruct);
    void WriteProperties(SStruct *pParentStruct, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc);
    void WriteProperty(SProperty *pProp, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc, bool IsOverride);
    void WritePropertyOverrides(SStruct *pStruct, SStruct *pCompareStruct, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc);
    void WriteEnumTemplate(SEnum *pEnum);
    void WriteEnumerators(SEnum *pEnum, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc);
    void WriteMaster();
    void WritePropertiesXML();
    void ReadNames();
    void MakeNames();
    QString NameFromError(const char *pkError, bool IsEnum);
};

#endif // CTEMPLATEWRITER_H
