#include "SProperty.h"
#include <iostream>

TString FloatToString(float f)
{
    TString out = std::to_string(f);
    while (out.Back() == '0') out = out.ChopBack(1);
    if (out.Back() == '.') out.Append('0');
    return out;
}

TString TypeAsString(EPropertyType type)
{
    switch (type)
    {
    case EPropertyType::Bool:           return "Bool";
    case EPropertyType::Byte:           return "Byte";
    case EPropertyType::Short:          return "Short";
    case EPropertyType::Int:            return "Int";
    case EPropertyType::Enum:           return "Enum";
    case EPropertyType::Float:          return "Float";
    case EPropertyType::String:         return "String";
    case EPropertyType::Vector3f:       return "Vector";
    case EPropertyType::Color:          return "Color";
    case EPropertyType::Asset:          return "Asset";
    case EPropertyType::MayaSpline:     return "Spline";
    case EPropertyType::LayerID:        return "LayerID";
    case EPropertyType::LayerSwitch:    return "LayerSwitch";
    case EPropertyType::Character:      return "AnimationSet";
    case EPropertyType::Struct:         return "Struct";
    case EPropertyType::Transform:      return "Transform";
    case EPropertyType::Array:          return "Array";
    default:                            return "Invalid";
    }
}

TString ValueAsString(SProperty *pProp)
{
    switch (pProp->Type)
    {
    case EPropertyType::Bool:
    {
        TBoolProperty *pBool = static_cast<TBoolProperty*>(pProp);
        return (pBool->DefaultValue ? "true" : "false");
    }
    case EPropertyType::Byte:
    {
        TByteProperty *pByte = static_cast<TByteProperty*>(pProp);
        return TString::FromInt32((int32) pByte->DefaultValue, 0, 10);
    }
    case EPropertyType::Short:
    {
        TShortProperty *pShort = static_cast<TShortProperty*>(pProp);
        return TString::FromInt32((int32) pShort->DefaultValue, 0, 10);
    }
    case EPropertyType::Int:
    {
        TIntProperty *pInt = static_cast<TIntProperty*>(pProp);
        return TString::FromInt32(pInt->DefaultValue, 0, 10);
    }
    case EPropertyType::Enum:
    {
        SEnum *pEnum = static_cast<SEnum*>(pProp);
        return TString::HexString((uint32) pEnum->IDForValue(pEnum->DefaultValue));
    }
    case EPropertyType::Float:
    {
        TFloatProperty *pFloat = static_cast<TFloatProperty*>(pProp);
        return FloatToString(pFloat->DefaultValue);
    }
    case EPropertyType::String:
        return "[string]";
    case EPropertyType::Vector3f:
    {
        TVectorProperty *pVector = static_cast<TVectorProperty*>(pProp);
        return pVector->DefaultValue.ToString();
    }
    case EPropertyType::Color:
    {
        TColorProperty *pColor = static_cast<TColorProperty*>(pProp);
        TString out;
        out += FloatToString(pColor->DefaultValue.r / 255.f) + ", ";
        out += FloatToString(pColor->DefaultValue.g / 255.f) + ", ";
        out += FloatToString(pColor->DefaultValue.b / 255.f) + ", ";
        out += FloatToString(pColor->DefaultValue.a / 255.f);
        return out;
    }
    case EPropertyType::Asset:
        return "[asset]";
    case EPropertyType::MayaSpline:
        return "[MayaSpline]";
    case EPropertyType::LayerID:
    {
        TLayerIDProperty *pLayer = static_cast<TLayerIDProperty*>(pProp);
        return TString::Format("%08X-%08X-%08X-%08X",
            (pLayer->DefaultValue.LayerID_A >> 32),
            (pLayer->DefaultValue.LayerID_A & 0xFFFFFFFF),
            (pLayer->DefaultValue.LayerID_B >> 32),
            (pLayer->DefaultValue.LayerID_B & 0xFFFFFFFF) );
    }
    case EPropertyType::LayerSwitch:
        return "[LayerSwitch]";
    case EPropertyType::Character:
        return "[Character]";
    case EPropertyType::Transform:
    {
        TTransformProperty *pTransform = static_cast<TTransformProperty*>(pProp);
        TString out = "Position=" + pTransform->DefaultValue.Position.ToString();
        out += " Rotation=" + pTransform->DefaultValue.Rotation.ToString();
        out += " Scale=" + pTransform->DefaultValue.Scale.ToString();
        return out;
    }
    case EPropertyType::Array:
        return "[Array]";
    case EPropertyType::Struct:
    {
        SStruct *pStruct = static_cast<SStruct*>(pProp);
        return TString::HexString(pStruct->LoaderAddress);//TString::FromInt32(pStruct->SubProperties.size(), 0, 10)  + " sub-properties";
    }
    default:
        return "INVALID PROPERTY";
    }
}

#define COMPARE_PROPERTIES(Type) static_cast<Type*>(pPropA)->DefaultValue == static_cast<Type*>(pPropB)->DefaultValue
bool DefaultsMatch(SProperty *pPropA, SProperty *pPropB)
{
    // Some int properties are actually assets - check if one has been retyped but not the other
    if (pPropA->Type == EPropertyType::Int && pPropB->Type == EPropertyType::Asset) return true;
    if (pPropA->Type == EPropertyType::Asset && pPropB->Type == EPropertyType::Int) return true;
    if (pPropA->Type != pPropB->Type) return false;

    switch (pPropA->Type)
    {
    // These property types match if the default value is the same
    case EPropertyType::Bool:       return COMPARE_PROPERTIES(TBoolProperty);
    case EPropertyType::Byte:       return COMPARE_PROPERTIES(TByteProperty);
    case EPropertyType::Short:      return COMPARE_PROPERTIES(TShortProperty);
    case EPropertyType::Int:        return COMPARE_PROPERTIES(TIntProperty);
    case EPropertyType::Float:      return COMPARE_PROPERTIES(TFloatProperty);
    case EPropertyType::Vector3f:   return COMPARE_PROPERTIES(TVectorProperty);
    case EPropertyType::Color:      return COMPARE_PROPERTIES(TColorProperty);
    case EPropertyType::Transform:  return COMPARE_PROPERTIES(TTransformProperty);
    case EPropertyType::Enum:       return COMPARE_PROPERTIES(SEnum);

    // These property types cannot have default overrides
    case EPropertyType::String:
    case EPropertyType::Asset:
    case EPropertyType::MayaSpline:
    case EPropertyType::LayerID:
    case EPropertyType::LayerSwitch:
    case EPropertyType::Character:
    case EPropertyType::Array:
        return true;

    // Structs match if all their sub-properties match
    case EPropertyType::Struct:
    {
        SStruct *pStructA = static_cast<SStruct*>(pPropA);
        SStruct *pStructB = static_cast<SStruct*>(pPropB);
        if (pStructA->SubProperties.size() != pStructB->SubProperties.size()) return false;

        for (int i=0; i<pStructA->SubProperties.size(); i++)
            if (!DefaultsMatch(pStructA->SubProperties[i], pStructB->SubProperties[i]))
                return false;

        return true;
    }

    default:
        return true;
    }
}

bool CheckErrors(SProperty *pProp)
{
    switch (pProp->Type)
    {
    case EPropertyType::String:
    {
        TStringProperty *pString = static_cast<TStringProperty*>(pProp);
        if ( (pString->DefaultValue.v0x04 != 0) || (pString->DefaultValue.v0x08 != 0) )
        {
            errorf("String property default has non-zero values");
            return false;
        } else return true;
    }

    case EPropertyType::Asset:
        if (static_cast<TAssetProperty*>(pProp)->DefaultValue != 0xFFFFFFFFFFFFFFFF)
        {
            errorf("Asset property default is not gkInvalidAssetId");
            return false;
        } else return true;

    case EPropertyType::MayaSpline:
    {
        TSplineProperty *pSpline = static_cast<TSplineProperty*>(pProp);

        if ( (pSpline->DefaultValue.v0x00 != 0) || (pSpline->DefaultValue.v0x04 != 0) || (pSpline->DefaultValue.v0x08 != 0) ||
             (pSpline->DefaultValue.v0x0C != -FLT_MAX) || (pSpline->DefaultValue.v0x10 != FLT_MAX) ||
             (pSpline->DefaultValue.v0x14 != false) || (pSpline->DefaultValue.v0x15 != false) || (pSpline->DefaultValue.v0x16 != false) ||
             (pSpline->DefaultValue.v0x17 != false) || (pSpline->DefaultValue.v0x18 != 65535) || (pSpline->DefaultValue.v0x1A != 65535) ||
             (pSpline->DefaultValue.v0x1C != 0.0f) )
        {
            errorf("MayaSpline default has some unexpected values set");
            return false;
        } else return true;
    }

    case EPropertyType::LayerID:
    {
        TLayerIDProperty *pID = static_cast<TLayerIDProperty*>(pProp);

        if ((pID->DefaultValue.LayerID_A != 0) || (pID->DefaultValue.LayerID_B != 0))
        {
            errorf("LayerID default has non-zero ID");
            return false;
        } else return true;
    }

    case EPropertyType::LayerSwitch:
    {
        TLayerSwitchProperty *pSwitch = static_cast<TLayerSwitchProperty*>(pProp);

        if ((pSwitch->DefaultValue.AreaID != 0xFFFFFFFFFFFFFFFF) || (pSwitch->DefaultValue.LayerID.LayerID_A != 0) || (pSwitch->DefaultValue.LayerID.LayerID_B != 0))
        {
            errorf("LayerSwitch default has unexpected values set");
            return false;
        } else return true;
    }

    case EPropertyType::Array:
    {
        TArrayProperty *pArray = static_cast<TArrayProperty*>(pProp);

        if ((pArray->DefaultValue.v0x0 != 0) || (pArray->DefaultValue.v0x4 != 0) || (pArray->DefaultValue.v0x8 != 0))
        {
            errorf("Array default has non-zero values set");
            return false;
        } else return true;
    }
    default:
        return true;
    }
}

template<class PropertyType>
SProperty* TemplatedCloneProperty(SProperty *pProp)
{
    PropertyType *pCastProp = static_cast<PropertyType*>(pProp);
    PropertyType *pOut = new PropertyType;
    pOut->DefaultValue = pCastProp->DefaultValue;
    return pOut;
}

SProperty* CloneProperty(SProperty *pProp)
{
    SProperty *pOut = nullptr;

    switch (pProp->Type)
    {
    case EPropertyType::Bool:           pOut = TemplatedCloneProperty<TBoolProperty>(pProp); break;
    case EPropertyType::Byte:           pOut = TemplatedCloneProperty<TByteProperty>(pProp); break;
    case EPropertyType::Short:          pOut = TemplatedCloneProperty<TShortProperty>(pProp); break;
    case EPropertyType::Int:            pOut = TemplatedCloneProperty<TIntProperty>(pProp); break;
    case EPropertyType::Float:          pOut = TemplatedCloneProperty<TFloatProperty>(pProp); break;
    case EPropertyType::Vector3f:       pOut = TemplatedCloneProperty<TVectorProperty>(pProp); break;
    case EPropertyType::Color:          pOut = TemplatedCloneProperty<TColorProperty>(pProp); break;
    case EPropertyType::Transform:      pOut = TemplatedCloneProperty<TTransformProperty>(pProp); break;
    case EPropertyType::String:         pOut = TemplatedCloneProperty<TStringProperty>(pProp); break;
    case EPropertyType::Asset:          pOut = TemplatedCloneProperty<TAssetProperty>(pProp); break;
    case EPropertyType::MayaSpline:     pOut = TemplatedCloneProperty<TSplineProperty>(pProp); break;
    case EPropertyType::LayerID:        pOut = TemplatedCloneProperty<TLayerIDProperty>(pProp); break;
    case EPropertyType::LayerSwitch:    pOut = TemplatedCloneProperty<TLayerSwitchProperty>(pProp); break;
    case EPropertyType::Character:      pOut = TemplatedCloneProperty<TCharacterProperty>(pProp); break;
    case EPropertyType::Array:          pOut = TemplatedCloneProperty<TArrayProperty>(pProp); break;

    case EPropertyType::Enum:
    {
        SEnum *pOutEnum = new SEnum;
        SEnum *pCastEnum = static_cast<SEnum*>(pProp);
        pOutEnum->Enumerators = pCastEnum->Enumerators;
        pOutEnum->DefaultValue = pCastEnum->DefaultValue;
        pOut = pOutEnum;
        break;
    }

    case EPropertyType::Struct:
    {
        SStruct *pOutStruct = new SStruct;
        SStruct *pCastStruct = static_cast<SStruct*>(pProp);
        pOutStruct->InitializerAddress = pCastStruct->InitializerAddress;
        pOutStruct->InitializerOffset = pCastStruct->InitializerOffset;
        pOut = pOutStruct;

        foreach (SProperty *pProp, pCastStruct->SubProperties)
        {
            pOutStruct->SubProperties << CloneProperty(pProp);
        }

        break;
    }
    }

    ASSERT(pOut);
    pOut->DataAddress = pProp->DataAddress;
    pOut->ID = pProp->ID;
    pOut->LoaderAddress = pProp->LoaderAddress;
    pOut->pkErrorString = pProp->pkErrorString;
    pOut->Type = pProp->Type;
    return pOut;
}
