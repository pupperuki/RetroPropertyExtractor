#ifndef SPROPERTY
#define SPROPERTY

#include <QVector>
#include <Common/BasicTypes.h>
#include <Common/TString.h>
#include <Common/Hash/CFNV1A.h>
#include <iostream>

enum class EPropertyType
{
    Invalid,
    Bool,
    Byte,
    Short,
    Int,
    Enum,
    Float,
    String,
    Vector3f,
    Color,
    Asset,
    Struct,
    Array,
    Character,
    LayerID,
    LayerSwitch,
    MayaSpline,
    Transform
};

struct SProperty
{
    uint32 ID;
    uint32 LoaderAddress;
    uint32 DataAddress;
    EPropertyType Type;
    const char *pkErrorString;

    SProperty() :
        ID(0), Type(EPropertyType::Invalid), LoaderAddress(0), DataAddress(0), pkErrorString(0) {}
};

struct SStruct : public SProperty
{
    uint32 InitializerAddress;
    int32 InitializerOffset; // For inlined initializers
    QVector<SProperty*> SubProperties;

    SStruct()
        : SProperty(), InitializerAddress(0), InitializerOffset(0) { Type = EPropertyType::Struct; }

    ~SStruct() {
        foreach (SProperty *pProp, SubProperties)
            delete pProp;
    }
};

struct SEnum : public SProperty
{
    struct SEnumerator
    {
        uint32 ID, Value;
        SEnumerator() : ID(0), Value(0) {}
    };
    QVector<SEnumerator> Enumerators;
    uint32 DefaultValue;

    SEnum()
        : SProperty(), DefaultValue(0) { Type = EPropertyType::Enum; }

    int32 IDForValue(uint32 value)
    {
        foreach (const SEnumerator& E, Enumerators)
            if (E.Value == value)
                return E.ID;

        errorf("ID requested from enum with invalid value: 0x%08X", value);
        return -1;
    }

    uint64 Hash()
    {
        CFNV1A Hash(CFNV1A::k64Bit);
        foreach(const SEnumerator& E, Enumerators)
        {
            Hash.HashLong(E.ID);
            Hash.HashLong(E.Value);
        }
        return Hash.GetHash64();
    }

    void SortEnumerators()
    {
        qSort(Enumerators.begin(), Enumerators.end(), [](const SEnumerator& kLHS, const SEnumerator& kRHS) -> bool {
            return (kLHS.Value < kRHS.Value);
        });
    }
};

// Value types
TString FloatToString(float f);

struct SString
{
    uint32 v0x00;
    uint32 v0x04;
    uint32 v0x08;

    bool operator==(const SString& kOther) {
        return (v0x00 == kOther.v0x00) && (v0x04 == kOther.v0x04) && (v0x08 == kOther.v0x08);
    }
};

struct SVector3f
{
    float x, y, z;

    TString ToString() {
        return FloatToString(x) + ", " + FloatToString(y) + ", " + FloatToString(z);
    }

    bool operator==(const SVector3f& kOther) {
        return (x == kOther.x) && (y == kOther.y) && (z == kOther.z);
    }

    bool operator!=(const SVector3f& kOther) {
        return (!(*this == kOther));
    }
};

struct SColor
{
    uint8 r, g, b, a;

    bool operator==(const SColor& kOther) {
        return (r == kOther.r) && (g == kOther.g) && (b == kOther.b) && (a == kOther.a);
    }
};

struct SMayaSpline
{
    uint32 v0x00;
    uint32 v0x04;
    uint32 v0x08;
    float v0x0C;
    float v0x10;
    bool v0x14;
    bool v0x15;
    bool v0x16;
    bool v0x17;
    uint16 v0x18;
    uint16 v0x1A;
    float v0x1C;

    bool operator==(const SMayaSpline& kOther) {
        return (v0x00 == kOther.v0x00) && (v0x04 == kOther.v0x04) && (v0x08 == kOther.v0x08) &&
               (v0x0C == kOther.v0x0C) && (v0x10 == kOther.v0x10) && (v0x14 == kOther.v0x14) &&
               (v0x15 == kOther.v0x15) && (v0x16 == kOther.v0x16) && (v0x17 == kOther.v0x17) &&
               (v0x18 == kOther.v0x18) && (v0x1A == kOther.v0x1A) && (v0x1C == kOther.v0x1C);
    }
};

struct SLayerID
{
    uint64 LayerID_A;
    uint64 LayerID_B;

    bool operator==(const SLayerID& kOther) {
        return (LayerID_A == kOther.LayerID_A) && (LayerID_B == kOther.LayerID_B);
    }
};

struct SLayerSwitch
{
    uint64 AreaID;
    SLayerID LayerID;

    bool operator==(const SLayerSwitch& kOther) {
        return (AreaID == kOther.AreaID) && (LayerID == kOther.LayerID);
    }
};

struct SCharacter
{
    uint64 AssetID;
    uint32 v0x08;
    uint32 v0x0C; // in sub-struct
    uint32 v0x10; // in sub-struct

    bool operator==(const SCharacter& kOther) {
        return (AssetID == kOther.AssetID) && (v0x08 == kOther.v0x08) &&
               (v0x0C == kOther.v0x0C) && (v0x10 == kOther.v0x10);
    }
};

struct STransform
{
    SVector3f Position;
    SVector3f Rotation;
    SVector3f Scale;

    bool operator==(const STransform& kOther) {
        return (Position == kOther.Position) && (Rotation == kOther.Rotation) && (Scale == kOther.Scale);
    }
};

struct SArray
{
    uint32 v0x0;
    uint32 v0x4;
    uint32 v0x8;

    bool operator==(const SArray& kOther) {
        return (v0x0 == kOther.v0x0) && (v0x4 == kOther.v0x4) && (v0x8 == kOther.v0x8);
    }
};

template<typename PropType>
struct STypedProperty : public SProperty
{
    PropType DefaultValue;
};

typedef STypedProperty<bool>         TBoolProperty;
typedef STypedProperty<int8>         TByteProperty;
typedef STypedProperty<int16>        TShortProperty;
typedef STypedProperty<int32>        TIntProperty;
typedef STypedProperty<float>        TFloatProperty;
typedef STypedProperty<SString>      TStringProperty;
typedef STypedProperty<SVector3f>    TVectorProperty;
typedef STypedProperty<SColor>       TColorProperty;
typedef STypedProperty<uint64>       TAssetProperty;
typedef STypedProperty<SMayaSpline>  TSplineProperty;
typedef STypedProperty<SLayerID>     TLayerIDProperty;
typedef STypedProperty<SLayerSwitch> TLayerSwitchProperty;
typedef STypedProperty<SCharacter>   TCharacterProperty;
typedef STypedProperty<STransform>   TTransformProperty;
typedef STypedProperty<SArray>       TArrayProperty;

TString TypeAsString(EPropertyType type);
TString ValueAsString(SProperty *pProp);
bool DefaultsMatch(SProperty *pPropA, SProperty *pPropB);
bool CheckErrors(SProperty *pProp);
SProperty* CloneProperty(SProperty *pProp);

#endif // SPROPERTY

