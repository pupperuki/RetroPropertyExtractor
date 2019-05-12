#include "CTemplateWriter.h"
#include <Common/FileIO.h>
#include <Common/FileUtil.h>
#include <iostream>
#include <QDir>

#define TO_QSTRING(str) QString::fromStdString((str).ToStdString())
using namespace tinyxml2;

void CTemplateWriter::MakeTemplates()
{
    mOutputDir = TO_QSTRING("GeneratedTemplates/" + mpLoader->GameIdentifier() + "/");
    mStructsDir = "Structs/";
    mEnumsDir = "Enums/";
    mScriptDir = "Script/";

    debugf("Saving templates to %s", mOutputDir.toStdString().c_str());

    QDir dir;
    dir.mkpath(mOutputDir + mStructsDir);
    dir.mkpath(mOutputDir + mEnumsDir);
    dir.mkpath(mOutputDir + mScriptDir);

    mStructNum = 0;
    mEnumNum = 0;
    mNoDefaults = false;

    // Find unique properties
    for (uint32 ObjIdx = 0; ObjIdx < mpLoader->NumObjects(); ObjIdx++)
    {
        SStruct *pBase = &mpLoader->Object(ObjIdx)->BaseStruct;

        // Don't allow defaults on tweaks as they don't have defaults
        CFourCC ObjID = mpLoader->Object(ObjIdx)->Type;
        //mNoDefaults = (ObjID[0] == 'T' && ObjID[1] == 'W');

        FindUniqueProperties(pBase);
    }

    // Load unique struct defaults
    uint32 NumExternalStructs = 0;
    QMutableMapIterator<uint32,SUniqueStruct> StructIt(mUniqueStructs);

    while (StructIt.hasNext())
    {
        SUniqueStruct *pStruct = &StructIt.next().value();
        if (!ShouldBeExternal(pStruct)) continue;
        NumExternalStructs++;
        mpLoader->LoadIndependentStruct(&pStruct->Struct);
    }

    // Load names
    if (!mpLoader->ShouldGetErrorStrings())
        ReadNames();
    else
        MakeNames();

    // Write struct templates
    StructIt.toFront();

    while (StructIt.hasNext())
    {
        SUniqueStruct *pStruct = &StructIt.next().value();
        if (!ShouldBeExternal(pStruct)) continue;

        if (pStruct->TemplatePath.isEmpty())
        {
            WriteStructTemplate(&pStruct->Struct);
        }
    }

    // Write enum templates
    QMutableMapIterator<uint64,SUniqueEnum> EnumIt(mUniqueEnums);

    while (EnumIt.hasNext())
    {
        SUniqueEnum *pEnum = &EnumIt.next().value();
        if (pEnum->NumReferences == 1) continue;

        if (pEnum->TemplatePath.isEmpty())
        {
            WriteEnumTemplate(&pEnum->Enum);
        }
    }

    // Write script templates
    for (uint32 ObjIdx = 0; ObjIdx < mpLoader->NumObjects(); ObjIdx++)
    {
        WriteScriptTemplate(mpLoader->Object(ObjIdx));
    }

    // Write master
    WriteMaster();
}

bool CTemplateWriter::ShouldBeExternal(SUniqueStruct *pStruct)
{
#if 1
    // updated template format mandates that all structs are external
    return true;
#else
    return ( (pStruct->NumReferences > 1) &&
             (pStruct->Struct.InitializerAddress != 0) &&
             (pStruct->Struct.LoaderAddress != 0) );
#endif
}

void CTemplateWriter::FindUniqueProperties(SStruct *pParentStruct)
{
    uint32 StructAddr = pParentStruct->LoaderAddress;

    if (StructAddr == 0 || !mParsedStructs.contains(StructAddr))
    {
        mParsedStructs.insert(StructAddr);

        foreach (SProperty *pProp, pParentStruct->SubProperties)
        {
            // Check unique enums
            if (pProp->Type == EPropertyType::Enum)
            {
                SEnum *pEnum = static_cast<SEnum*>(pProp);

                if (!mUniqueEnums.contains(pEnum->Hash()))
                {
                    SUniqueEnum Enum;
                    Enum.NumReferences = 1;
                    Enum.Enum.Type = EPropertyType::Enum;
                    Enum.Enum.pkErrorString = pEnum->pkErrorString;

                    mUniqueEnums.insert(pEnum->Hash(), Enum);
                    mUniqueEnums[pEnum->Hash()].Enum.Enumerators = pEnum->Enumerators;
                }

                else
                {
                    mUniqueEnums[pEnum->Hash()].NumReferences++;
                }
            }

            // Check unique structs
            else if (pProp->Type == EPropertyType::Struct)
            {
                SStruct *pStruct = static_cast<SStruct*>(pProp);
                FindUniqueProperties(pStruct);

                if (!mUniqueStructs.contains(pStruct->LoaderAddress))
                {
                    SUniqueStruct& Struct = mUniqueStructs[pStruct->LoaderAddress];
                    Struct.NumReferences = 1;
                    Struct.Struct.pkErrorString = pStruct->pkErrorString;
                    Struct.Struct.InitializerAddress = pStruct->InitializerAddress;
                    Struct.Struct.InitializerOffset = pStruct->InitializerOffset;
                    Struct.Struct.LoaderAddress = pStruct->LoaderAddress;
                    Struct.Struct.Type = EPropertyType::Struct;
                    Struct.NoDefaults = mNoDefaults;
                    if (Struct.Struct.LoaderAddress == 0x801A11B0) Struct.Struct.InitializerAddress = 0x801A10D0; // Hack for CameraHint

                    // The only case where InitializerAddress is 0 is if the initializer was inlined and the parent is a script object
                    // In this case, we cannot perform an independent load of the struct, as the data will be on the stack and we can't retrieve it
                    // However, this case also generally only occurs on structs that are only referenced a single time
                    // So, work around it by just copying the values from the original struct instead of re-loading it
                    if (Struct.Struct.InitializerAddress == 0)
                    {
                        foreach (SProperty *pProp, pStruct->SubProperties)
                        {
                            Struct.Struct.SubProperties << CloneProperty(pProp);
                        }
                    }
                }

                else
                {
                    mUniqueStructs[pStruct->LoaderAddress].NumReferences++;
                }
            }
        }
    }
}

void CTemplateWriter::WriteScriptTemplate(SObjectInfo *pObj)
{
    QString Name = QString::fromStdString(pObj->Type.ToString().ToStdString());
    QString Type = TO_QSTRING(pObj->Type.ToString());

    if (mpLoader->ShouldGetErrorStrings() && mNames.contains(Type))
        Name = mNames[Type];

    else if (mNames.contains(Name))
        Name = mNames[Name];

    QString OutPath = mOutputDir + mScriptDir + Name + ".xml";

    XMLDocument Doc;
    XMLDeclaration *pDecl = Doc.NewDeclaration();
    Doc.LinkEndChild(pDecl);

    XMLElement *pScriptTemplate = Doc.NewElement("ScriptObject");
    pScriptTemplate->SetAttribute("ArchiveVer", "4");
    pScriptTemplate->SetAttribute("Game", *mpLoader->GameIdentifier());
    Doc.LinkEndChild(pScriptTemplate);

    XMLElement *pPropertiesElement = Doc.NewElement("Properties");
    pPropertiesElement->SetAttribute("Type", "Struct");
    pScriptTemplate->LinkEndChild(pPropertiesElement);

    XMLElement *pName = Doc.NewElement("Name");
    pName->SetText(Name.toStdString().c_str());
    pPropertiesElement->LinkEndChild(pName);

    // Don't allow defaults on tweaks as they don't have defaults
    bool IsTweaks = (pObj->Type[0] == 'T' && pObj->Type[1] == 'W');
   // mNoDefaults = IsTweaks;

    WriteProperties(&pObj->BaseStruct, pPropertiesElement, &Doc);

    // Editor properties
    if (!IsTweaks && pObj->Type != "SNFO")
    {
        XMLElement *pEditor = Doc.NewElement("EditorProperties");
        pScriptTemplate->LinkEndChild(pEditor);

        static const TString EdPropNames[6] = {
            "NameProperty", "PositionProperty", "RotationProperty",
            "ScaleProperty", "ActiveProperty", "LightParametersProperty"
        };

        static const TString EdPropIDs[6] = {
            "0x255A4580:0x494E414D", "0x255A4580:0x5846524D:0x00", "0x255A4580:0x5846524D:0x01",
            "0x255A4580:0x5846524D:0x02", "0x255A4580:0x41435456", "0x7E397FED:0xB028DB0E"
        };

        for (uint32 PropIdx = 0; PropIdx < 6; PropIdx++)
        {
            // Name, Transform, and Active are all present on every object. LightParameters isn't though, so need to check for it
            if (PropIdx == 5)
            {
                bool HasLightParameters = false;

                foreach (SProperty *pProp, pObj->BaseStruct.SubProperties)
                {
                    if (pProp->ID == 0x7E397FED) // ID for ActorParameters, which contains LightParameters
                    {
                        HasLightParameters = true;
                        break;
                    }
                }

                if (!HasLightParameters) break;
            }

            XMLElement *pProp = Doc.NewElement(*EdPropNames[PropIdx]);
            pProp->SetText(*EdPropIDs[PropIdx]);
            pEditor->LinkEndChild(pProp);
        }
    }

    Doc.SaveFile(OutPath.toStdString().c_str());
}

void CTemplateWriter::WriteStructTemplate(SStruct *pStruct)
{
    SUniqueStruct& UniqueStruct = mUniqueStructs[pStruct->LoaderAddress];
    QString Name = "Struct" + QString::number(mStructNum);

    if (!UniqueStruct.ProtoName.isEmpty())
        Name = UniqueStruct.ProtoName;

    else if (mNames.contains(Name))
        Name = mNames[Name];

    QString OutPath = mStructsDir + Name + ".xml";
    QString AbsPath = mOutputDir + OutPath;
    mStructNum++;

    XMLDocument Doc;
    XMLDeclaration *pDecl = Doc.NewDeclaration();
    Doc.LinkEndChild(pDecl);

    XMLElement *pRoot = Doc.NewElement("PropertyTemplate");
    pRoot->SetAttribute("ArchiveVer", "4");
    pRoot->SetAttribute("Game", *mpLoader->GameIdentifier());

    XMLElement *pPropertyArchetype = Doc.NewElement("PropertyArchetype");
    pPropertyArchetype->SetAttribute("Type", "Struct");
    pRoot->LinkEndChild(pPropertyArchetype);

    XMLElement *pName = Doc.NewElement("Name");
    pName->SetText(Name.toStdString().c_str());
    pPropertyArchetype->LinkEndChild(pName);

    WriteProperties(pStruct, pPropertyArchetype, &Doc);
    Doc.LinkEndChild(pRoot);
    Doc.SaveFile(AbsPath.toStdString().c_str());

    UniqueStruct.TemplatePath = OutPath;
}

void CTemplateWriter::WriteProperties(SStruct *pParentStruct, XMLElement *pRootElem, XMLDocument *pDoc)
{
    XMLElement *pPropsElem = pDoc->NewElement("SubProperties");
    pRootElem->LinkEndChild(pPropsElem);

    foreach (SProperty *pProp, pParentStruct->SubProperties)
        WriteProperty(pProp, pPropsElem, pDoc, false);
}

void CTemplateWriter::WriteProperty(SProperty *pProp, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc, bool IsOverride)
{
    // Make element
    XMLElement *pElem = pDoc->NewElement("Element");
    pElem->SetAttribute("Type", *TypeAsString(pProp->Type));
    pElem->SetAttribute("ID", *TString::HexString(pProp->ID));
    pRootElem->LinkEndChild(pElem);

    // Struct
    if (pProp->Type == EPropertyType::Struct)
    {
        SStruct *pStruct = static_cast<SStruct*>(pProp);
        SUniqueStruct& UniqueStruct = mUniqueStructs[pStruct->LoaderAddress];

        if (ShouldBeExternal(&UniqueStruct))
        {
            if (UniqueStruct.TemplatePath.isEmpty())
                WriteStructTemplate(&UniqueStruct.Struct);

            pElem->SetAttribute("Archetype", *TString(UniqueStruct.TemplatePath.toStdString()).GetFileName(false));
            WritePropertyOverrides(pStruct, &UniqueStruct.Struct, pElem, pDoc);
        }

        else
        {
            WriteProperties(pStruct, pElem, pDoc);
        }
    }

    // Enum
    else if (pProp->Type == EPropertyType::Enum)
    {
        SEnum *pEnum = static_cast<SEnum*>(pProp);
        SUniqueEnum& UniqueEnum = mUniqueEnums[pEnum->Hash()];

        if (!mNoDefaults)
        {
            TString ValueString = ValueAsString(pProp);

            if (ValueString != "0xFFFFFFFF")
            {
                XMLElement *pDefault = pDoc->NewElement("DefaultValue");
                pElem->LinkEndChild(pDefault);
                pDefault->SetText(*ValueAsString(pProp));
            }
            else
            {
                // In cases where the enum has an invalid default value (this actually happens in a few places)
                // we cannot preserve the real default value so instead force it to always cook
                XMLElement *pCookPref = pDoc->NewElement("CookPreference");
                pElem->LinkEndChild(pCookPref);
                pCookPref->SetText("Always");
            }
        }

        // note we'd like all enums to have archetype templates
        if (true)//(rUniqueEnum.NumReferences > 1)
        {
            if (UniqueEnum.TemplatePath.isEmpty())
                WriteEnumTemplate(&UniqueEnum.Enum);

            pElem->SetAttribute("Archetype", *TString(UniqueEnum.TemplatePath.toStdString()).GetFileName(false));
        }

        else
        {
            XMLElement *pEnumers = pDoc->NewElement("Values");
            pElem->LinkEndChild(pEnumers);
            WriteEnumerators(pEnum, pEnumers, pDoc);
        }
    }

    // Array
    else if (pProp->Type == EPropertyType::Array)
    {
    }

    // Layer ID
    else if (pProp->Type == EPropertyType::LayerID)
        pElem->SetAttribute("Archetype", "LayerID");

    // Layer Switch
    else if (pProp->Type == EPropertyType::LayerSwitch)
        pElem->SetAttribute("Archetype", "LayerSwitch");

    // Transform
    else if (pProp->Type == EPropertyType::Transform)
        pElem->SetAttribute("Archetype", "Transform");

    // Property
    else
    {
        // Check whether this int property is actually an asset
        if (pProp->Type == EPropertyType::Int)
            if (mExtensions.contains(pProp->ID))
                pProp->Type = EPropertyType::Asset;

        if ( (pProp->Type != EPropertyType::String) &&
             (pProp->Type != EPropertyType::Asset) &&
             (pProp->Type != EPropertyType::Character) &&
             (pProp->Type != EPropertyType::MayaSpline) )
        {
            if (!mNoDefaults)
            {
                XMLElement *pDefault = pDoc->NewElement("DefaultValue");
                pElem->LinkEndChild(pDefault);
                pDefault->SetText(*ValueAsString(pProp));
            }
        }

        if (pProp->Type == EPropertyType::Asset)
        {
            XMLElement *pTypeFilter = pDoc->NewElement("TypeFilter");
            pElem->LinkEndChild(pTypeFilter);

            TString exts = (mExtensions.contains(pProp->ID) ? mExtensions[pProp->ID] : "UNKN");
            TStringList extsList = exts.Split(",");

            for (auto Iter = extsList.begin(); Iter != extsList.end(); Iter++)
            {
                XMLElement* pExt = pDoc->NewElement("Element");
                pExt->SetText(**Iter);
                pTypeFilter->LinkEndChild(pExt);
            }
         }
    }

    // Add unique property
    if (!mUniqueProperties.contains(pProp->ID))
    {
        // Check for proto name - otherwise default to Unknown
        if (pProp->pkErrorString)
            mUniqueProperties[pProp->ID] = NameFromError(pProp->pkErrorString, (pProp->Type == EPropertyType::Enum));

        else
            mUniqueProperties[pProp->ID] = "Unknown";
    }
 }

void CTemplateWriter::WritePropertyOverrides(SStruct *pStruct, SStruct *pCompareStruct, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc)
{
    if (mNoDefaults)
        return;

    if (!DefaultsMatch(pStruct, pCompareStruct))
    {
        XMLElement *pProps = pDoc->NewElement("SubProperties");
        pRootElem->LinkEndChild(pProps);

        for (int iProp = 0; iProp < pStruct->SubProperties.size(); iProp++)
        {
            SProperty *pProp = pStruct->SubProperties[iProp];
            SProperty *pCompareProp = pCompareStruct->SubProperties[iProp];

            if (!DefaultsMatch(pProp, pCompareProp))
            {
                TString ID = TString::HexString(pProp->ID);

                if (pProp->Type == EPropertyType::Struct)
                {
                    XMLElement *pElem = pDoc->NewElement("Element");
                    pElem->SetAttribute("Type", *TypeAsString(pProp->Type));
                    pElem->SetAttribute("ID", *ID);
                    pProps->LinkEndChild(pElem);

                    WritePropertyOverrides(static_cast<SStruct*>(pProp), static_cast<SStruct*>(pCompareProp), pElem, pDoc);
                }

                else if (pProp->Type == EPropertyType::Enum)
                {
                    XMLElement *pElem = pDoc->NewElement("Element");
                    pElem->SetAttribute("Type", *TypeAsString(pProp->Type));
                    pElem->SetAttribute("ID", *ID);
                    pProps->LinkEndChild(pElem);

                    if (!mNoDefaults)
                    {
                        XMLElement *pDefault = pDoc->NewElement("DefaultValue");
                        pElem->LinkEndChild(pDefault);
                        pDefault->SetText(*ValueAsString(pProp));
                    }
                }

                else if (pProp->Type == EPropertyType::Transform)
                {
                    TTransformProperty *pTransform = static_cast<TTransformProperty*>(pProp);
                    TTransformProperty *pCompareTransform = static_cast<TTransformProperty*>(pCompareProp);

                    XMLElement *pElem = pDoc->NewElement("Element");
                    pElem->SetAttribute("Type", *TypeAsString(pProp->Type));
                    pElem->SetAttribute("ID", *ID);
                    pProps->LinkEndChild(pElem);

                    XMLElement *pSubProps = pDoc->NewElement("SubProperties");
                    pElem->LinkEndChild(pSubProps);

                    SVector3f SubProps[3] = {
                        pTransform->DefaultValue.Position,
                        pTransform->DefaultValue.Rotation,
                        pTransform->DefaultValue.Scale
                                         };

                    SVector3f CompareSubProps[3] = {
                        pCompareTransform->DefaultValue.Position,
                        pCompareTransform->DefaultValue.Rotation,
                        pCompareTransform->DefaultValue.Scale
                    };

                    for (uint32 SubIdx = 0; SubIdx < 3; SubIdx++)
                    {
                        if (SubProps[SubIdx] != CompareSubProps[SubIdx])
                        {
                            XMLElement *pSubPropElem = pDoc->NewElement("Element");
                            pSubPropElem->SetAttribute("Type", "Vector");
                            pSubPropElem->SetAttribute("ID", *TString::HexString(SubIdx, 2));
                            pSubProps->LinkEndChild(pSubPropElem);

                            if (!mNoDefaults)
                            {
                                XMLElement *pDefault = pDoc->NewElement("DefaultValue");
                                pDefault->SetText(*SubProps[SubIdx].ToString());
                                pSubPropElem->LinkEndChild(pDefault);
                            }
                        }
                    }
                }

                else
                {
                    // Check for asset properties marked as int
                    if (pProp->Type == EPropertyType::Int && mExtensions.contains(pProp->ID))
                        continue;

                    WriteProperty(pProp, pProps, pDoc, true);
                }
            }
        }
    }
}

void CTemplateWriter::WriteEnumTemplate(SEnum *pEnum)
{
    SUniqueEnum& UniqueEnum = mUniqueEnums[pEnum->Hash()];
    QString Name = "Enum" + QString::number(mEnumNum);

    if (!UniqueEnum.ProtoName.isEmpty())
        Name = UniqueEnum.ProtoName;

    else if (mNames.contains(Name))
        Name = mNames[Name];

    QString OutPath = mEnumsDir + Name + ".xml";
    QString AbsPath = mOutputDir + OutPath;
    mEnumNum++;

    XMLDocument Doc;
    XMLDeclaration *pDecl = Doc.NewDeclaration();
    Doc.LinkEndChild(pDecl);

    XMLElement *pRoot = Doc.NewElement("PropertyTemplate");
    pRoot->SetAttribute("ArchiveVersion", "4");
    pRoot->SetAttribute("Game", *mpLoader->GameIdentifier());

    XMLElement *pPropertyArchetype = Doc.NewElement("PropertyArchetype");
    pPropertyArchetype->SetAttribute("Type", "Enum");
    pRoot->LinkEndChild(pPropertyArchetype);

    XMLElement *pName = Doc.NewElement("Name");
    pName->SetText(Name.toStdString().c_str());
    pPropertyArchetype->LinkEndChild(pName);

    WriteEnumerators(pEnum, pPropertyArchetype, &Doc);
    Doc.LinkEndChild(pRoot);
    Doc.SaveFile(AbsPath.toStdString().c_str());

    UniqueEnum.TemplatePath = OutPath;
}

void CTemplateWriter::WriteEnumerators(SEnum *pEnum, tinyxml2::XMLElement *pRootElem, tinyxml2::XMLDocument *pDoc)
{
    uint32 Num = 0;
    XMLElement *pValues = pDoc->NewElement("Values");
    pRootElem->LinkEndChild(pValues);

    foreach (const SEnum::SEnumerator& Enumer, pEnum->Enumerators)
    {
        Num++;

        XMLElement *pElem = pDoc->NewElement("Element");
        pValues->LinkEndChild(pElem);

        TString ID = TString::HexString(Enumer.ID);
        TString NumStr = "Unknown" + TString::FromInt32(Num, 0, 10);
        pElem->SetAttribute("Name", *NumStr);
        pElem->SetAttribute("ID", *ID);
    }
}

void CTemplateWriter::ReadNames()
{
    QString ListFiles[3] = { "EnumNames.txt", "StructNames.txt", "ObjNames.txt" };

    for (uint32 i = 0; i < 3; i++)
    {
        FILE* f = fopen((mOutputDir + ListFiles[i]).toStdString().c_str(), "r");
        char Buffer[256];

        if (f)
        {
            while (!feof(f))
            {
                memset(&Buffer[0], 0, 256);
                fgets(&Buffer[0], 256, f);

                TString str = Buffer;
                TStringList strings = str.Split(" \n\t");
                if (strings.size() != 2)
                    errorf("Error reading from %s; there's a line with less/more than one space", ListFiles[i].toStdString().c_str());
                mNames[TO_QSTRING(strings.front())] = TO_QSTRING(strings.back());
            }

            fclose(f);
        }
    }

    TString ProtoPath = "GeneratedTemplates/CorruptionProto/";

    if (FileUtil::Exists(ProtoPath))
    {
        QMap<uint32,QString> IdToName;

        // grab names from MP3 proto
        TStringList XMLs;
        FileUtil::GetDirectoryContents(ProtoPath, XMLs);

        for (auto Iter = XMLs.begin(); Iter != XMLs.end(); Iter++)
        {
            TString Path = *Iter;

            if (Path.EndsWith(".xml"))
            {
                XMLDocument Doc;
                XMLError Error = Doc.LoadFile(*Path);
                if (Error != XML_SUCCESS) continue;

                XMLElement *pRootElem = Doc.FirstChildElement();
                if (!pRootElem) continue;

                // Check type
                XMLElement* pProperties = nullptr;

                if (strcmp(pRootElem->Name(), "ScriptObject") == 0)
                {
                    pProperties = pRootElem->FirstChildElement("Properties");
                }
                else if (strcmp(pRootElem->Name(), "PropertyTemplate") == 0)
                {
                    pProperties = pRootElem->FirstChildElement("PropertyArchetype");
                }

                if (!pProperties) continue;
                if (strcmp(pProperties->Attribute("Type"), "Struct") != 0) continue;

                pProperties = pProperties->FirstChildElement("SubProperties");
                ASSERT( pProperties );

                for (XMLElement *pChild = pProperties->FirstChildElement(); pChild; pChild = pChild->NextSiblingElement())
                {
                    if (pChild->Attribute("Archetype") != nullptr && pChild->Attribute("ID") != nullptr)
                    {
                        uint32 ID = TString( pChild->Attribute("ID") ).ToInt32(16);
                        QString Name = pChild->Attribute("Archetype");

                        ASSERT( !IdToName.contains(ID) || IdToName[ID] == Name );
                        IdToName[ID] = Name;
                    }
                }
            }
        }

        // map these IDs back to our names
        for (uint i=0; i<mpLoader->NumObjects(); i++)
        {
            SObjectInfo* pInfo = mpLoader->Object(i);

            foreach (SProperty* pProperty, pInfo->BaseStruct.SubProperties)
            {
                if (pProperty->Type == EPropertyType::Struct || pProperty->Type == EPropertyType::Enum)
                {
                    if (IdToName.contains(pProperty->ID))
                    {
                        QString Name = IdToName[pProperty->ID];

                        if (pProperty->Type == EPropertyType::Struct)
                        {
                            ASSERT( pProperty->LoaderAddress != 0 );
                            ASSERT( mUniqueStructs.contains(pProperty->LoaderAddress) );
                            SUniqueStruct& Struct = mUniqueStructs[pProperty->LoaderAddress];
                            ASSERT( Struct.ProtoName.isEmpty() || Struct.ProtoName == Name );
                            Struct.ProtoName = Name;
                        }
                        else
                        {
                            uint64 EnumHash = static_cast<SEnum*>(pProperty)->Hash();
                            ASSERT( mUniqueEnums.contains(EnumHash) );
                            SUniqueEnum& Enum = mUniqueEnums[EnumHash];
                            ASSERT( Enum.ProtoName.isEmpty() || Enum.ProtoName == Name );
                            Enum.ProtoName = Name;
                        }
                    }
                }
            }
        }

        QMutableMapIterator<uint32, SUniqueStruct> StructIt(mUniqueStructs);

        while (StructIt.hasNext())
        {
            StructIt.next();
            SUniqueStruct& Struct = StructIt.value();

            foreach (SProperty* pProperty, Struct.Struct.SubProperties)
            {
                if (pProperty->Type == EPropertyType::Struct || pProperty->Type == EPropertyType::Enum)
                {
                    if (IdToName.contains(pProperty->ID))
                    {
                        QString Name = IdToName[pProperty->ID];

                        if (pProperty->Type == EPropertyType::Struct)
                        {
                            ASSERT( pProperty->LoaderAddress != 0 );
                            ASSERT( mUniqueStructs.contains(pProperty->LoaderAddress) );
                            SUniqueStruct& Struct = mUniqueStructs[pProperty->LoaderAddress];
                            ASSERT( Struct.ProtoName.isEmpty() || Struct.ProtoName == Name );
                            Struct.ProtoName = Name;
                        }
                        else
                        {
                            uint64 EnumHash = static_cast<SEnum*>(pProperty)->Hash();
                            ASSERT( mUniqueEnums.contains(EnumHash) );
                            SUniqueEnum& Enum = mUniqueEnums[EnumHash];
                            ASSERT( Enum.ProtoName.isEmpty() || Enum.ProtoName == Name );
                            Enum.ProtoName = Name;
                        }
                    }
                }
            }
        }
    }

#if 0
    QString PropsXmlName = mOutputDir + "OldProperties.xml";
    tinyxml2::XMLDocument Properties;
    Properties.LoadFile(PropsXmlName.toStdString().c_str());

    XMLElement *pRoot = Properties.FirstChildElement("Properties");
    XMLElement *pElem = pRoot->FirstChildElement();

    while (pElem)
    {
        TString ID = pElem->Attribute("ID");
        TString Name = pElem->Attribute("name");
        mUniqueProperties[ID.ToInt32(16)] = TO_QSTRING(Name);

        // Check for file extensions
        const char *pkExt = pElem->Attribute("ext");

        if (pkExt)
        {
            uint32 IntID = ID.ToInt32();
            if (!mExtensions.contains(IntID))
                mExtensions[IntID] = pkExt;
        }

        pElem = pElem->NextSiblingElement();
    }
#endif
}

void CTemplateWriter::MakeNames()
{
    for (uint32 ObjIdx = 0; ObjIdx < mpLoader->NumObjects(); ObjIdx++)
    {
        SObjectInfo *pObj = mpLoader->Object(ObjIdx);
        mNames[TO_QSTRING(pObj->Type.ToString())] = NameFromError(pObj->BaseStruct.pkErrorString, false);
    }

    QMutableMapIterator<uint32,SUniqueStruct> StructIter(mUniqueStructs);
    while (StructIter.hasNext())
    {
        StructIter.next();
        SUniqueStruct& Struct = StructIter.value();
        Struct.ProtoName = NameFromError(Struct.Struct.pkErrorString, false);
    }

    QMutableMapIterator<uint64,SUniqueEnum> EnumIter(mUniqueEnums);
    while (EnumIter.hasNext())
    {
        EnumIter.next();
        SUniqueEnum& Enum = EnumIter.value();
        Enum.ProtoName = NameFromError(Enum.Enum.pkErrorString, true);
    }
}

QString CTemplateWriter::NameFromError(const char *pkError, bool IsEnum)
{
    QRegExp skStructRegExp("^Unknown property 0x%08x in (.*) loader.\n$");
    QRegExp skEnumRegExp("^Unknown enum hash 0x%08x for (.*) in .* loader.\n$");

    const QRegExp *kpRegExp = (IsEnum ? &skEnumRegExp : &skStructRegExp);
    kpRegExp->indexIn(QString(pkError));
    return kpRegExp->cap(1);
}

void CTemplateWriter::WriteMaster()
{
    XMLDocument Doc;
    XMLDeclaration *pDecl = Doc.NewDeclaration();
    Doc.LinkEndChild(pDecl);

    XMLElement *pMaster = Doc.NewElement("MasterTemplate");
    pMaster->SetAttribute("version", 4);
    Doc.LinkEndChild(pMaster);

    XMLElement *pProps = Doc.NewElement("properties");
    pProps->SetText("Properties.xml");
    pMaster->LinkEndChild(pProps);

    XMLElement *pObjs = Doc.NewElement("objects");
    pMaster->LinkEndChild(pObjs);

    for (uint32 ObjIdx = 0; ObjIdx < mpLoader->NumObjects(); ObjIdx++)
    {
        SObjectInfo *pInfo = mpLoader->Object(ObjIdx);
        TString ObjName = pInfo->Type.ToString();
        if (mNames.contains(TO_QSTRING(ObjName))) ObjName = mNames[TO_QSTRING(ObjName)].toStdString();

        XMLElement *pObj = Doc.NewElement("object");
        pObj->SetAttribute("ID", *pInfo->Type.ToString());
        pObj->SetAttribute("template", *("Script/" + ObjName + ".xml"));
        pObjs->LinkEndChild(pObj);
    }

    XMLElement *pStates = Doc.NewElement("states");
    pMaster->LinkEndChild(pStates);

    XMLElement *pMessages = Doc.NewElement("messages");
    pMaster->LinkEndChild(pMessages);

    Doc.SaveFile( (mOutputDir + "MasterTemplate.xml").toStdString().c_str() );
    WritePropertiesXML();
}

void CTemplateWriter::WritePropertiesXML()
{
    XMLDocument Doc;
    XMLDeclaration *pDecl = Doc.NewDeclaration();
    Doc.LinkEndChild(pDecl);

    XMLElement *pProps = Doc.NewElement("Properties");
    pProps->SetAttribute("version", 4);
    Doc.LinkEndChild(pProps);

    QMapIterator<uint32,QString> Iter(mUniqueProperties);

    while (Iter.hasNext())
    {
        Iter.next();
        uint32 PropID = Iter.key();
        QString PropName = Iter.value();

        TString StrID = TString::HexString(PropID);
        TString StrName = PropName.toStdString();

        XMLElement *pProp = Doc.NewElement("property");
        pProp->SetAttribute("ID", *StrID);
        pProp->SetAttribute("name", *StrName);
        pProps->LinkEndChild(pProp);
    }

    QString Out = mOutputDir + "Properties.xml";
    Doc.SaveFile(Out.toStdString().c_str());
}
