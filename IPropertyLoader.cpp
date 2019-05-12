#include "IPropertyLoader.h"
#include "Masks.h"
#include <iostream>

IPropertyLoader::IPropertyLoader(CByteCodeReader *pReader, TString DolPath, TString FSTRoot)
    : mpReader(pReader)
    , mDolPath(DolPath)
    , mFSTRoot(FSTRoot)
{
}

IPropertyLoader::~IPropertyLoader()
{
}

void IPropertyLoader::LoadObjectLoaders()
{
    uint32 Address = ObjectsAddress();
    SObjectInfo Obj;
    uint32 typeInt;

    while (true)
    {
        mpReader->Load(Address, &typeInt, 4);
        mpReader->Load(Address + 4, &Obj.LoaderAddress, 4);
        if (!typeInt) break;

        Obj.Type = typeInt;
        mObjects << Obj;
        Address += 8;
    }
}

void IPropertyLoader::ParseObjectLoaders()
{
    uint32 InitialStack = mpReader->GetRegisterValue(1);

    for (int ObjIdx = 0; ObjIdx < mObjects.size(); ObjIdx++)
    {
        SObjectInfo& Info = mObjects[ObjIdx];
        debugf("%d. Parsing properties for %s at 0x%08X", ObjIdx, *Info.Type.ToString(), Info.LoaderAddress);

        // Point r3 to a valid memory address.
        // On most objects this will be ignored since the object is constructed on the stack, but it is needed for tweaks, which load into r3
        mpReader->SetRegisterValue(3, 0x80C00000);

        // If the object has an external initializer, run it now
        uint32 ExternalInitializer = GetExternalInitializerAddress(Info.Type);

        if (ExternalInitializer > 0)
        {
            ParseExternalInitializer(&mLinkMap[Info.Type.ToLong()], ExternalInitializer);
            mpReader->SetRegisterValue(3, 0x80C00000);
        }

        // Parse the beginning of the loader function and initialize defaults if applicable
        ParseStructLoader(&mLinkMap[Info.Type.ToLong()], Info.LoaderAddress, false);

        // Check for separate initializer
        if (HasSeparateInitializerFunction(Info.Type))
            mLinkMap[Info.Type.ToLong()] = mLinkMap[Info.Type.ToLong()].SubStructs.first();

        ParseStructIDs(&Info.BaseStruct, &mLinkMap[Info.Type.ToLong()]);
        SortProperties(&Info.BaseStruct);

        if (Info.BaseStruct.SubProperties.front()->ID != 0x255A4580)
            warnf("EditorProperties isn't first! Incorrect data address somewhere?");

        //DebugPrintProperties(&Info.BaseStruct);
        mpReader->SetRegisterValue(1, InitialStack);
    }
}

void IPropertyLoader::ParseExternalInitializer(SLinkedStructInfo *pLinkInfo, uint32 Address)
{
    InitStructLoader(Address, false);
    uint32 InitializerDepth = 0;

    // Store a pointer to the current SLinkedStructInfo to keep track
    SLinkedStructInfo *pCurLinkInfo = pLinkInfo;
    pCurLinkInfo->InitializerAddress = Address;
    bool done = false;

    while (!done)
    {
        SInstruction Instr = mpReader->ParseInstruction();
        uint32 Params = Instr.Parameters;
        if (Instr.OpCode == UnknownOpCode) break;

        // Check for branching into a struct initializer or a call to non-inlined CInputStream::ReadLong
        if (Instr.OpCode == b)
        {
            int32 LI = ApplyMask(Params, 6, 29, false, true);
            bool AA = ApplyMask(Params, 30, 30) == 1;
            bool LK = ApplyMask(Params, 31, 31) == 1;
            uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);

            if (IsReadFunction(TargetAddr) || IsSaveRestGPRFunction(TargetAddr))
            {
                if (!ExecuteReadFunction(TargetAddr))
                    mpReader->ExecuteInstruction(Instr);
            }

            else
            {
                pCurLinkInfo = pCurLinkInfo ? LinkStruct(pCurLinkInfo, mpReader->GetRegisterValue(3), TargetAddr) : nullptr;
                mpReader->GoToAddress(TargetAddr, true);
                InitializerDepth++;
            }
        }

        else if (Instr.OpCode == bclr && InitializerDepth == 0)
        {
            done = true;
        }

        // Continue executing to load the default params.
        else
        {
            bool done = mpReader->ExecuteInstruction(Instr);
            if (done)
            {
                if (InitializerDepth == 0)
                {
                    errorf("Unexpected end of function while loading defaults at 0x%08X", mpReader->CurrentAddress());
                    break;
                }
                else
                {
                    InitializerDepth--;
                    pCurLinkInfo = pCurLinkInfo ? pCurLinkInfo->pParent : nullptr;
                }
            }
        }
    }
}

void IPropertyLoader::InitStructLoader(uint32 Address, bool IsIndependent)
{
    mpReader->GoToAddress(Address);
    mPropertyStates.clear();

    // Create dummy input stream at 0x400 in bss and point r4 to it so we can track what register has the input stream/property ID.
    // 0x8 within the input stream structure is a pointer to the data.
    // Input stream is at 0x400, pointer is at 0x408, property ID dummy is at 0x40C; function starts with input stream on base struct size (so two bytes before the prop ID)
    mInputStreamAddress = mpReader->GetBssAddress() + 0x400;
    uint32 StreamPointer = mInputStreamAddress + 0xA;
    uint32 PropID = mskDummyPropID;
    mpReader->Store(mInputStreamAddress + 0x8, &StreamPointer, 4);
    mpReader->Store(mInputStreamAddress + 0xC, &PropID, 4);
    mpReader->SetRegisterValue(4, mInputStreamAddress);
}

void IPropertyLoader::ParseStructLoader(SLinkedStructInfo *pLinkInfo, uint32 Address, bool IsIndependent)
{
    InitStructLoader(Address, IsIndependent);
    bool LoadingStarted = IsIndependent;
    uint32 InitializerDepth = 0;

    // Store a pointer to the current SLinkedStructInfo to keep track
    SLinkedStructInfo *pCurLinkInfo = pLinkInfo;
    bool done = false;

    while (!done)
    {
        SInstruction Instr = mpReader->ParseInstruction();
        uint32 Params = Instr.Parameters;
        if (Instr.OpCode == UnknownOpCode) break;

        if (!LoadingStarted)
        {
            // The loading may be done in a separate function. Look for an addi.
            if (Instr.OpCode == addi)
            {
                LoadingStarted = true;
                QString ModuleName = mpReader->FindAddressModule(mpReader->CurrentAddress());
                //debugf("Module: %s", *TString(ModuleName.toStdString()));
            }

            else if (Instr.OpCode == b)
            {
                int32 LI = ApplyMask(Params, 6, 29, false, true);
                bool AA = ApplyMask(Params, 30, 30) == 1;
                uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);

                if (IsReadFunction(TargetAddr) || IsSaveRestGPRFunction(TargetAddr))
                {
                    ExecuteReadFunction(TargetAddr);
                }
                else
                {
                    mpReader->GoToAddress(TargetAddr);
                }
            }

            // For bcctr, make sure LK is unset so we don't execute the entire function at the branch
            else if (Instr.OpCode == bcctr)
                Instr.Parameters &= ~0x1;

            if (Instr.OpCode != b)
                mpReader->ExecuteInstruction(Instr);
        }

        else
        {
            // Check for branching into a struct initializer or a call to non-inlined CInputStream::ReadLong
            if (Instr.OpCode == b)
            {
                int32 LI = ApplyMask(Params, 6, 29, false, true);
                bool AA = ApplyMask(Params, 30, 30) == 1;
                bool LK = ApplyMask(Params, 31, 31) == 1;
                uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);

                if ((InitializerDepth == 0) && !LK)
                    done = true;

                else if (IsReadFunction(TargetAddr) || IsSaveRestGPRFunction(TargetAddr))
                {
                    if (!ExecuteReadFunction(TargetAddr))
                        mpReader->ExecuteInstruction(Instr);
                }

                else
                {
                    pCurLinkInfo = pCurLinkInfo ? LinkStruct(pCurLinkInfo, mpReader->GetRegisterValue(3), TargetAddr) : nullptr;
                    mpReader->GoToAddress(TargetAddr, true);
                    InitializerDepth++;
                }
            }

            // Continue executing to load the default params.
            else
            {
                bool done = mpReader->ExecuteInstruction(Instr);
                if (done)
                {
                    if (InitializerDepth == 0)
                    {
                        if (!IsIndependent)
                            errorf("Unexpected end of function while loading defaults at 0x%08X", mpReader->CurrentAddress());
                        break;
                    }
                    else
                    {
                        InitializerDepth--;
                        pCurLinkInfo = pCurLinkInfo ? pCurLinkInfo->pParent : nullptr;
                    }
                }
            }
        }
    }
}

void IPropertyLoader::ParseStructIDs(SStruct *pStruct, SLinkedStructInfo *pLinkInfo)
{
    bool Done = false;

    SProperty Property;
    uint32 InvalidPropBranch = 0;

    QVector<uint32> SubBranches;
    QVector<SReaderState> SubBranchStates;
    int LoopedSubBranches = 0;

    // There are two patterns used to check property IDs (the full ID from the file is in r0):
    // Pattern 1 is addis r0, r6 [Upper Half] / cmplwi r0, [Lower Half]
    // Pattern 2 is lis rXX, [Upper Half] / addi rYY, rXX [Lower Half] / cmpw r0, rYY
    while (!Done)
    {
        SInstruction instr = mpReader->ParseInstruction();
        uint32 Params = instr.Parameters;
        if (instr.OpCode == UnknownOpCode) break;

        if (instr.OpCode == addis)
        {
            uint32 rA = ApplyMask(Params, 11, 15);
            int16 SIMM = (int16) ApplyMask(Params, 16, 31);

            // If this operation is being done on the property ID, then this is pattern 1
            if (mpReader->GetRegisterValue(rA) == mskDummyPropID)
            {
                Property.ID &= 0x0000FFFF;

                if (SIMM < 0)
                    Property.ID |= (-SIMM << 16);
                else if (SIMM > 0)
                    Property.ID |= ((0x10000 - SIMM) << 16);
            }

            // Otherwise, it might be pattern 2 or might be unrelated; execute it either way to make sure registers are correct
            mpReader->ExecuteInstruction(instr);
        }

        else if ((instr.OpCode == addi) || (instr.OpCode == lwz) || (instr.OpCode == or_))
        {
            // Execute addi, lwz, and OR to make sure the register values are correct
            mpReader->ExecuteInstruction(instr);
        }

        else if (instr.OpCode == cmp)
        {
            // Check for compare against the property ID (r0)
            uint32 rA = ApplyMask(Params, 11, 15);
            uint32 rB = ApplyMask(Params, 16, 20);

            if (mpReader->GetRegisterValue(rA) == mskDummyPropID)
                Property.ID = mpReader->GetRegisterValue(rB);
        }

        else if (instr.OpCode == cmpli)
        {
            uint32 rA = ApplyMask(Params, 11, 15);
            uint32 UIMM = ApplyMask(Params, 16, 31);

            if ((mpReader->GetRegisterValue(rA) & 0xFFFF) == (mskDummyPropID & 0xFFFF))
                Property.ID |= (uint32) UIMM;
        }

        else if (instr.OpCode == bc)
        {
            // This might be beq (branch to property loader), bne (used to branch to
            // invalid property on objects with one property), or bge (extra subbranch).
            uint32 BO = ApplyMask(Params, 6, 10);
            uint32 BI = ApplyMask(Params, 11, 15);
            bool AA = ApplyMask(Params, 30, 30) == 1;

            int32 BD = ApplyMask(Params, 16, 29, false, true);
            uint32 TargetAddr = (AA ? (uint32) BD : mpReader->CurrentAddress() + BD);

            // bne
            if ((BO & 0x1C) == 4 && BI == 2)
            {
                InvalidPropBranch = TargetAddr;
                Property.LoaderAddress = mpReader->CurrentAddress() + 4;
                pStruct->SubProperties << new SProperty(Property);
                mPropertyStates[Property.LoaderAddress] = mpReader->SaveState();
                Property = SProperty();
                Done = true;
            }

            // beq
            else if ((BO & 0x1C) == 0xC && BI == 2)
            {
                // Add property and clear
                Property.LoaderAddress = TargetAddr;
                pStruct->SubProperties << new SProperty(Property);
                mPropertyStates[Property.LoaderAddress] = mpReader->SaveState();
                Property = SProperty();
            }

            // bge
            else if ((BO & 0x1C) == 4 && BI == 0)
            {
                SubBranches << TargetAddr;
                SubBranchStates << mpReader->SaveState();
            }
        }

        else if (instr.OpCode == b)
        {
            // Check whether this is a call to CInputStream::ReadLong or ReadShort
            int32 LI = ApplyMask(Params, 6, 29, false, true);
            bool AA = ApplyMask(Params, 30, 30) == 1;
            uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);

            if (!IsReadFunction(TargetAddr))
            {
                // It's not, so this is a branch to invalid property after all other properties have been checked
                if (!InvalidPropBranch)
                    InvalidPropBranch = TargetAddr;

                // Check whether we have any valid subbranches. If not we're done
                while (LoopedSubBranches < SubBranches.size())
                {
                    if (SubBranches[LoopedSubBranches] == InvalidPropBranch)
                        LoopedSubBranches++;
                    else
                        break;
                }

                if (LoopedSubBranches < SubBranches.size())
                {
                    mpReader->LoadState(SubBranchStates[LoopedSubBranches]);
                    mpReader->GoToAddress(SubBranches[LoopedSubBranches]);
                    LoopedSubBranches++;
                }
                else
                    Done = true;
            }

            // If it is a CInputStream call then execute it
            else
            {
                if (!ExecuteReadFunction(TargetAddr))
                    mpReader->ExecuteInstruction(instr);
            }
        }
    }

    // Get the struct error string in the MP3 prototype
    if (ShouldGetErrorStrings())
    {
        mpReader->GoToAddress(InvalidPropBranch);

        while (true)
        {
            SInstruction Instr = mpReader->ParseInstruction();

            if (Instr.OpCode == b)
            {
                pStruct->pkErrorString = mpReader->PointerToAddress(mpReader->GetRegisterValue(3));
                break;
            }

            else if ((Instr.OpCode == addis) || (Instr.OpCode == addi))
                mpReader->ExecuteInstruction(Instr);
        }
    }

    ParseStructTypes(pStruct, pLinkInfo);
}

void IPropertyLoader::ParseStructTypes(SStruct *pStruct, SLinkedStructInfo *pLinkInfo)
{
    foreach(SProperty* pProp, pStruct->SubProperties)
    {
        mpReader->LoadState(mPropertyStates[pProp->LoaderAddress]);
        mpReader->GoToAddress(pProp->LoaderAddress);
        bool done = false;
        bool IsInPropLoader = false;

        while (!done)
        {
            SInstruction instr = mpReader->ParseInstruction();
            uint32 Params = instr.Parameters;
            bool ShouldExecute = true;

            // Check for cmp, cmpi, and cmpli, which may indicate an inline enum
            if ( ((instr.OpCode == cmp) || (instr.OpCode == cmpi) || (instr.OpCode == cmpli)) &&
                 (pProp->Type == EPropertyType::Invalid || pProp->Type == EPropertyType::Int))
            {
                pProp = ReadInlineEnum(pStruct, pProp);
                break;
            }

            // Look for srwi shifting 31 bits, which indicates a bool property.
            if (instr.OpCode == rlwinm)
            {
                uint32 SH = ApplyMask(Params, 16, 20);
                uint32 MB = ApplyMask(Params, 21, 25);
                uint32 ME = ApplyMask(Params, 26, 30);

                if ((ME == 31) && ((32 - MB) == SH))
                {
                    if (MB == 31)
                        pProp->Type = EPropertyType::Bool;
                    else
                        warnf("srwi %d", MB);
                }
            }

            // Don't execute lbz, lhz, lwz, or lfs from the input stream
            if ((instr.OpCode == lbz) || (instr.OpCode == lhz) || (instr.OpCode == lwz))
            {
                ShouldExecute = false;
            }

            // Look for stb, sth, stw, and stfs to indicate bool/byte/short/int/float properties.
            if ((instr.OpCode == stb) || (instr.OpCode == sth) || (instr.OpCode == stw) || (instr.OpCode == stfs))
            {
                uint32 rA = ApplyMask(Params, 11, 15);
                int16 d = (int16) ApplyMask(Params, 16, 31);
                uint32 EA = mpReader->GetRegisterValue(rA) + d;

                // Check that this isn't a write to the input stream
                if (!IsInputStreamRegister(rA))
                {
                    ShouldExecute = false;

                    // Assign type for invalid property
                    if (pProp->Type == EPropertyType::Invalid)
                    {
                        if (instr.OpCode == stb)
                            pProp->Type = EPropertyType::Byte;
                        else if (instr.OpCode == sth)
                            pProp->Type = EPropertyType::Short;
                        else if (instr.OpCode == stw)
                            pProp->Type = EPropertyType::Int;
                        else if (instr.OpCode == stfs)
                            pProp->Type = EPropertyType::Float;
                    }

                    // Assign data address (check for less-than to deal with multiple stfs calls on vectors)
                    if ((pProp->DataAddress == 0) || (EA < pProp->DataAddress))
                    {
                        pProp->DataAddress = EA;
                        ASSERT(pProp->DataAddress > 0x80000000);
                    }
                }
            }

            // Check for branch return from property loader
            if (instr.OpCode == bclr)
            {
                IsInPropLoader = false;
            }

            // Check branches, which can potentially tell us about a bunch of different property types
            if (instr.OpCode == b)
            {
                ShouldExecute = false;

                int32 LI = ApplyMask(Params, 6, 29, false, true);
                bool AA = ApplyMask(Params, 30, 30) == 1;
                bool LK = ApplyMask(Params, 31, 31) == 1;
                uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);

                // If LK is unset, then this is a local branch, meaning this is the end of the property loader
                if (!LK && !IsInPropLoader)
                    done = true;

                // Check if this is a separate property loader function.
                else if (IsPropLoader(TargetAddr))
                {
                    mpReader->GoToAddress(TargetAddr, LK);
                    IsInPropLoader = true;
                }

                // Otherwise check the branch to determine what kind of property this is.
                else
                {
                    EPropertyType type = PropTypeForLoader(TargetAddr);

                    // Struct - create a new SStruct, create a reference to it, and parse its loader function to get the IDs/types of subproperties
                    if (type == EPropertyType::Struct)
                    {
                        SLinkedStructInfo *pSubStructLinkInfo = nullptr;
                        int32 InitializerOffset = 0;

                        if (pLinkInfo)
                        {
                            if (HasInlinedInitializer(TargetAddr))
                            {
                                pSubStructLinkInfo = pLinkInfo;

                                // Set initializer offset, or if we can't (since we don't know the parent's data address yet), flag to calculate later
                                if (pStruct->DataAddress && pStruct->InitializerOffset != -1)
                                    InitializerOffset = (mpReader->GetRegisterValue(3) - pStruct->DataAddress) + pStruct->InitializerOffset;
                                else
                                    InitializerOffset = -1;
                            }
                            else if (pLinkInfo->SubStructs.contains(mpReader->GetRegisterValue(3)))
                                pSubStructLinkInfo = &pLinkInfo->SubStructs[mpReader->GetRegisterValue(3)];
                            else
                            {
                                pSubStructLinkInfo = pLinkInfo;
                                warnf("Error branching to struct loader at 0x%08X; couldn't find linked info for substruct", mpReader->CurrentAddress());
                            }
                        }

                        // Create new struct + delete old property
                        SStruct *pSubStruct = new SStruct();
                        pSubStruct->DataAddress = mpReader->GetRegisterValue(3);
                        pSubStruct->ID = pProp->ID;
                        pSubStruct->LoaderAddress = TargetAddr;
                        pSubStruct->InitializerAddress = pSubStructLinkInfo ? pSubStructLinkInfo->InitializerAddress : 0;
                        pSubStruct->InitializerOffset = InitializerOffset;

                        pStruct->SubProperties.removeOne(pProp);
                        pStruct->SubProperties.append(pSubStruct);
                        delete pProp;
                        pProp = (SProperty*) pSubStruct;

                        // Go to address, execute until the first local branch, then call ParseStructIDs
                        uint32 Address = mpReader->CurrentAddress();
                        mpReader->GoToAddress(TargetAddr, true);
                        mpReader->SetRegisterValue(4, mInputStreamAddress);
                        ResetInputStream();

                        while (true)
                        {
                            SInstruction BranchInstr = mpReader->ParseInstruction();

                            if (BranchInstr.OpCode == b)
                            {
                                bool SubLK = ApplyMask(BranchInstr.Parameters, 31, 31) == 1;
                                if (!SubLK) break;

                                int32 SubLI = ApplyMask(BranchInstr.Parameters, 6, 29, false, true);
                                bool SubAA = ApplyMask(BranchInstr.Parameters, 30, 30) == 1;
                                uint32 SubTargetAddr = (SubAA ? (uint32) SubLI : mpReader->CurrentAddress() + SubLI);

                                if (!IsReadFunction(SubTargetAddr) || !ExecuteReadFunction(SubTargetAddr))
                                    mpReader->ExecuteInstruction(BranchInstr);
                            }

                            else mpReader->ExecuteInstruction(BranchInstr);
                        }

                        ParseStructIDs(pSubStruct, pSubStructLinkInfo);
                        mpReader->GoToAddress(Address + 4, false);

                        if (pSubStruct->ID == 0)
                            errorf("Struct ID is 0");
                        if (pSubStruct->SubProperties.size() == 0)
                            errorf("Struct with no sub-properties; loader at 0x%08X", TargetAddr);
                    }

                    // Enum - Parse loader entries
                    else if (type == EPropertyType::Enum)
                    {
                        SEnum *pEnum = new SEnum();
                        pEnum->DataAddress = mpReader->GetRegisterValue(3);
                        pEnum->ID = pProp->ID;
                        pEnum->LoaderAddress = pProp->LoaderAddress;

                        pStruct->SubProperties.removeOne(pProp);
                        pStruct->SubProperties.append(pEnum);
                        delete pProp;
                        pProp = (SProperty*) pEnum;

                        // Parse enumerator entries
                        uint32 EntriesAddress = mpReader->GetRegisterValue(5);
                        uint32 NumEntries = mpReader->GetRegisterValue(6);

                        for (uint32 EntryIdx = 0; EntryIdx < NumEntries; EntryIdx++)
                        {
                            SEnum::SEnumerator enumerator;
                            mpReader->Load(EntriesAddress + 0, &enumerator.ID, 4);
                            mpReader->Load(EntriesAddress + 4, &enumerator.Value, 4);
                            pEnum->Enumerators << enumerator;
                            EntriesAddress += 8;
                        }

                        pEnum->SortEnumerators();

                        // Load default
                        mpReader->Load(pEnum->DataAddress, &pEnum->DefaultValue, 4);

                        // Now we have all the info we need, so if we're in a loader function, we can return now
                        if (IsInPropLoader)
                        {
                            IsInPropLoader = false;
                            done = true;
                        }
                    }

                    // Otherwise, just mark type
                    else if (type != EPropertyType::Invalid)
                    {
                        pProp->Type = type;

                        if ( (pProp->Type == EPropertyType::String) ||
                             (pProp->Type == EPropertyType::MayaSpline) ||
                             (pProp->Type == EPropertyType::LayerSwitch) ||
                             (pProp->Type == EPropertyType::Character) ||
                             (pProp->Type == EPropertyType::Transform) ||
                             (pProp->Type == EPropertyType::Array) /*|| (pProp->Type == EPropertyType::Color)*/)
                        {
                            pProp->DataAddress = mpReader->GetRegisterValue(3);
                        }
                        else
                        {
                            pProp->DataAddress = 0; // Reset data address so any prior store instructions won't count
                        }

                        // For MayaSpline, if this is a constructor call, then we might end up branching to weird places,
                        // so just break here - we already have all the info we need anyway
                        if (pProp->Type == EPropertyType::MayaSpline)
                            done = true;
                    }
                }
            }

            // Execute based on whether ShouldExecute has been set
            if (ShouldExecute) mpReader->ExecuteInstruction(instr);
        }

        if (pProp->Type == EPropertyType::Invalid)
        {
            errorf("Parsed loader for property 0x%08X at 0x%08X and couldn't determine type", pProp->ID, pProp->LoaderAddress);
        }
        else if ((pProp->Type != EPropertyType::Enum) && (pProp->Type != EPropertyType::Struct))
        {
            MakeProperty(pProp, pStruct, pProp->Type);
        }
    }

    // Calculate any initializer offsets required
    if (!pStruct->DataAddress)
    {
        foreach(SProperty *pProp, pStruct->SubProperties)
        {
            if (!pStruct->DataAddress || pStruct->DataAddress > pProp->DataAddress)
                pStruct->DataAddress = pProp->DataAddress;
        }
        CalculateStructInitializerOffsets(pStruct);
    }
    //debugf("struct done");
}

void IPropertyLoader::CalculateStructInitializerOffsets(SStruct *pStruct)
{
    foreach(SProperty *pProp, pStruct->SubProperties)
    {
        if (pProp->Type == EPropertyType::Struct)
        {
            SStruct *pSubStruct = static_cast<SStruct*>(pProp);

            if (pSubStruct->InitializerOffset == -1)
            {
                pSubStruct->InitializerOffset = (pSubStruct->DataAddress - pStruct->DataAddress) + pStruct->InitializerOffset;
                CalculateStructInitializerOffsets(pSubStruct);
            }
        }
    }
}

void IPropertyLoader::LoadIndependentStruct(SStruct *pStruct)
{
    uint32 InitialStack = mpReader->GetRegisterValue(1);
    if (pStruct->LoaderAddress == 0 || pStruct->InitializerAddress == 0)
    {
        warnf("Unable to load independent struct; %s address is 0", (pStruct->InitializerAddress == 0 ? "initializer" : "loader"));
        return;
    }

    uint32 StructDataAddr = 0x80C00000;

    // Load defaults
    if (pStruct->InitializerAddress)
    {
        mpReader->SetRegisterValue(3, StructDataAddr - pStruct->InitializerOffset);
        ResetInputStream();
        ParseStructLoader(nullptr, pStruct->InitializerAddress ? pStruct->InitializerAddress : pStruct->LoaderAddress, true);
    }

    // Go to loader, skip til first branch
    mpReader->GoToAddress(pStruct->LoaderAddress);
    mpReader->SetRegisterValue(1, InitialStack);
    mpReader->SetRegisterValue(3, StructDataAddr);
    mpReader->SetRegisterValue(4, mInputStreamAddress);
    ResetInputStream();

    while (true)
    {
        SInstruction Instr = mpReader->ParseInstruction();

        if (Instr.OpCode == b)
        {
            int32 LI = ApplyMask(Instr.Parameters, 6, 29, false, true);
            bool AA = ApplyMask(Instr.Parameters, 30, 30) == 1;
            bool LK = ApplyMask(Instr.Parameters, 31, 31) == 1;
            uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);
            if (!LK) break;

            if (!IsReadFunction(TargetAddr) || !ExecuteReadFunction(TargetAddr))
                mpReader->ExecuteInstruction(Instr);
        }

        else mpReader->ExecuteInstruction(Instr);
    }
    ParseStructIDs(pStruct, nullptr);
    mpReader->SetRegisterValue(1, InitialStack);

    SortProperties(pStruct);
}

SProperty* IPropertyLoader::ReadInlineEnum(SStruct *pStruct, SProperty *pProp)
{
    mpReader->LoadState(mPropertyStates[pProp->LoaderAddress]);
    ResetInputStream();
    mpReader->GoToAddress(pProp->LoaderAddress);

    struct SInlineEnumerator
    {
        uint32 ID;
        uint32 LoaderAddress;
        SReaderState State;

        SInlineEnumerator() : ID(0), LoaderAddress(0) {}
    };
    QVector<SInlineEnumerator> Enumerators;

    bool Done = false;
    SInlineEnumerator InlineEnumerator;
    QVector<uint32> SubBranches;
    QVector<SReaderState> SubBranchStates;
    int32 LoopedSubBranches = 0;
    uint32 InvalidEnumerBranch = 0;

    // Gather enumerator IDs
    while (!Done)
    {
        SInstruction instr = mpReader->ParseInstruction();
        uint32 Params = instr.Parameters;
        bool ShouldExecute = false;

        // addis - load upper 16 bits of ID
        if (instr.OpCode == addis)
        {
            int16 SIMM = (int16) ApplyMask(Params, 16, 31);
            InlineEnumerator.ID &= 0x0000FFFF;

            if (SIMM < 0)
                InlineEnumerator.ID |= (-SIMM << 16);
            else if (SIMM > 0)
                InlineEnumerator.ID |= ((0x10000 - SIMM) << 16);

            ShouldExecute = true;
        }

        // addi, lwz, and OR - execute to make sure the register values are correct
        else if ((instr.OpCode == addi) || (instr.OpCode == lwz) || (instr.OpCode == or_))
        {
            ShouldExecute = true;
        }

        // cmp - enumerator ID is in rB
        else if (instr.OpCode == cmp)
        {
            uint32 rB = ApplyMask(Params, 16, 20);
            InlineEnumerator.ID = mpReader->GetRegisterValue(rB);
        }

        // cmpli - compare lower 16 bits of ID
        else if (instr.OpCode == cmpli)
        {
            uint32 UIMM = ApplyMask(Params, 16, 31);
            InlineEnumerator.ID |= (uint32) UIMM;
        }

        // bc - register enumerator
        else if (instr.OpCode == bc)
        {
            // This might be beq, bne, or bge.
            uint32 BO = ApplyMask(Params, 6, 10);
            uint32 BI = ApplyMask(Params, 11, 15);
            bool AA = ApplyMask(Params, 30, 30) == 1;

            int32 BD = ApplyMask(Params, 16, 29, false, true);
            uint32 TargetAddr = (AA ? (uint32) BD : mpReader->CurrentAddress() + BD);

            // bne - branch to invalid enumerator
            if ((BO & 0x1C) == 4 && BI == 2)
            {
                InvalidEnumerBranch = TargetAddr;
                InlineEnumerator.LoaderAddress = mpReader->CurrentAddress() + 4;
                Enumerators << InlineEnumerator;
                Enumerators.back().State = mpReader->SaveState();
                InlineEnumerator = SInlineEnumerator();
                Done = true;
            }

            // beq - branch to enumerator loader
            else if ((BO & 0x1C) == 0xC && BI == 2)
            {
                InlineEnumerator.LoaderAddress = TargetAddr;
                Enumerators << InlineEnumerator;
                Enumerators.back().State = mpReader->SaveState();
                InlineEnumerator = SInlineEnumerator();
            }

            // bge - extra subbranch
            else if ((BO & 0x1C) == 4 && BI == 0)
            {
                SubBranches << TargetAddr;
                SubBranchStates << mpReader->SaveState();
            }
        }

        // b - branch to prop loader OR enumerators over, branch to invalid enumerator
        else if (instr.OpCode == b)
        {
            int32 LI = ApplyMask(Params, 6, 29, false, true);
            bool AA = ApplyMask(Params, 30, 30) == 1;
            uint32 TargetAddr = (AA ? (uint32) LI : mpReader->CurrentAddress() + LI);

            if (IsPropLoader(TargetAddr))
                mpReader->GoToAddress(TargetAddr);

            else
            {
                if (!InvalidEnumerBranch)
                    InvalidEnumerBranch = TargetAddr;

                // Check whether we have any valid subbranches. If not we're done
                while (LoopedSubBranches < SubBranches.size())
                {
                    if (SubBranches[LoopedSubBranches] == InvalidEnumerBranch)
                        LoopedSubBranches++;
                    else
                        break;
                }

                if (LoopedSubBranches < SubBranches.size())
                {
                    mpReader->LoadState(SubBranchStates[LoopedSubBranches]);
                    mpReader->GoToAddress(SubBranches[LoopedSubBranches]);
                    LoopedSubBranches++;
                }
                else
                    Done = true;
            }
        }

        if (ShouldExecute)
            mpReader->ExecuteInstruction(instr);
    }

    // Make enum property
    SEnum *pEnum = new SEnum();
    pEnum->ID = pProp->ID;
    pEnum->LoaderAddress = pProp->LoaderAddress;

    pStruct->SubProperties.removeOne(pProp);
    pStruct->SubProperties.append(pEnum);
    delete pProp;

    // Load enumerators into pEnum
    foreach (const SInlineEnumerator& kEnumer, Enumerators)
    {
        SEnum::SEnumerator Enumerator;
        Enumerator.ID = kEnumer.ID;

        mpReader->LoadState(kEnumer.State);
        mpReader->GoToAddress(kEnumer.LoaderAddress);

        while (true)
        {
            // Go until a stw instruction
            SInstruction instr = mpReader->ParseInstruction();

            if (instr.OpCode == stw)
            {
                uint32 Params = instr.Parameters;

                uint32 rS = ApplyMask(Params, 6, 10);
                uint32 rA = ApplyMask(Params, 11, 15);
                int16 d = (int16) ApplyMask(Params, 16, 31);
                uint32 EA = mpReader->GetRegisterValue(rA) + d;

                if (!pEnum->DataAddress)
                {
                    pEnum->DataAddress = EA;
                    mpReader->Load(EA, &pEnum->DefaultValue, 4);
                }

                Enumerator.Value = mpReader->GetRegisterValue(rS);
                pEnum->Enumerators << Enumerator;
                break;
            }

            else mpReader->ExecuteInstruction(instr);
        }
    }

    pEnum->SortEnumerators();

    // Get the enum error string in the MP3 prototype
    if (ShouldGetErrorStrings())
    {
        mpReader->GoToAddress(InvalidEnumerBranch);

        while (true)
        {
            SInstruction Instr = mpReader->ParseInstruction();

            if (Instr.OpCode == b)
            {
                pEnum->pkErrorString = mpReader->PointerToAddress(mpReader->GetRegisterValue(3));
                break;
            }

            else if ((Instr.OpCode == addis) || (Instr.OpCode == addi))
                mpReader->ExecuteInstruction(Instr);
        }
    }

    return pEnum;
}

uint32 IPropertyLoader::NumObjects()
{
    return mObjects.size();
}

SObjectInfo* IPropertyLoader::Object(uint32 index)
{
    return &mObjects[index];
}

IPropertyLoader::SLinkedStructInfo* IPropertyLoader::LinkStruct(SLinkedStructInfo *pLinkInfo, uint32 DataAddress, uint32 InitializerAddress)
{
    if (pLinkInfo->SubStructs.contains(DataAddress))
    {
        warnf("LinkStruct called on the same data address multiple times");
    }

    else
    {
        SLinkedStructInfo info;
        info.InitializerAddress = InitializerAddress;
        info.pParent = pLinkInfo;
        pLinkInfo->SubStructs[DataAddress] = info;
        ASSERT(info.InitializerAddress != 0);
    }

    return &pLinkInfo->SubStructs[DataAddress];
}

void IPropertyLoader::ResetInputStream()
{
    uint32 StreamPointer = mInputStreamAddress + 0xA;
    mpReader->Store(mInputStreamAddress + 0x8, &StreamPointer, 4);
}

bool IPropertyLoader::IsInputStreamRegister(uint32 RegID)
{
    return (mpReader->GetRegisterValue(RegID) == mInputStreamAddress);
}

void IPropertyLoader::SortProperties(SStruct *pStruct)
{
    foreach (SProperty *pProp, pStruct->SubProperties)
        if (pProp->Type == EPropertyType::Struct)
            SortProperties(static_cast<SStruct*>(pProp));

    qSort(pStruct->SubProperties.begin(), pStruct->SubProperties.end(),
          [](SProperty *pPropA, SProperty *pPropB) -> bool {
        return (pPropA->DataAddress < pPropB->DataAddress);
    });
}

void IPropertyLoader::DebugPrintProperties(SStruct *pStruct, uint32 Depth /*= 0*/)
{
    foreach (SProperty *pProp, pStruct->SubProperties)
    {
        TString Arrow;

        for (uint32 i=0; i<=Depth; i++)
            Arrow += ">";

        debugf("%s 0x%08X: %s - %s", *Arrow, pProp->ID, *TypeAsString(pProp->Type), *ValueAsString(pProp));

        if (pProp->Type == EPropertyType::Struct)
            DebugPrintProperties(static_cast<SStruct*>(pProp), Depth + 1);
    }
}

SProperty* IPropertyLoader::MakeProperty(SProperty *pBase, SStruct *pStruct, EPropertyType Type)
{
    SProperty *pOut;
    uint32 DataAddress = pBase->DataAddress;

    switch (Type)
    {
    case EPropertyType::Bool: {
        TBoolProperty *pBool = new TBoolProperty();
        mpReader->Load(DataAddress, &pBool->DefaultValue, 1);
        pOut = (SProperty*) pBool;
        break;
    }
    case EPropertyType::Byte: {
        TByteProperty *pByte = new TByteProperty();
        mpReader->Load(DataAddress, &pByte->DefaultValue, 1);
        pOut = (SProperty*) pByte;
        break;
    }
    case EPropertyType::Short: {
        TShortProperty *pShort = new TShortProperty();
        mpReader->Load(DataAddress, &pShort->DefaultValue, 2);
        pOut = (SProperty*) pShort;
        break;
    }
    case EPropertyType::Int: {
        TIntProperty *pInt = new TIntProperty();
        mpReader->Load(DataAddress, &pInt->DefaultValue, 4);
        pOut = (SProperty*) pInt;
        break;
    }
    case EPropertyType::Float: {
        TFloatProperty *pFloat = new TFloatProperty();
        mpReader->Load(DataAddress, &pFloat->DefaultValue, 4);
        pOut = (SProperty*) pFloat;
        break;
    }
    case EPropertyType::String: {
        TStringProperty *pString = new TStringProperty();
        mpReader->Load(DataAddress + 0, &pString->DefaultValue.v0x00, 4);
        mpReader->Load(DataAddress + 4, &pString->DefaultValue.v0x04, 4);
        mpReader->Load(DataAddress + 8, &pString->DefaultValue.v0x08, 4);
        pOut = (SProperty*) pString;
        break;
    }
    case EPropertyType::Vector3f: {
        TVectorProperty *pVector = new TVectorProperty();
        mpReader->Load(DataAddress + 0, &pVector->DefaultValue.x, 4);
        mpReader->Load(DataAddress + 4, &pVector->DefaultValue.y, 4);
        mpReader->Load(DataAddress + 8, &pVector->DefaultValue.z, 4);
        pOut = (SProperty*) pVector;
        break;
    }
    case EPropertyType::Color: {
        TColorProperty *pColor = new TColorProperty();
        mpReader->Load(DataAddress + 0, &pColor->DefaultValue.r, 1);
        mpReader->Load(DataAddress + 1, &pColor->DefaultValue.g, 1);
        mpReader->Load(DataAddress + 2, &pColor->DefaultValue.b, 1);
        mpReader->Load(DataAddress + 3, &pColor->DefaultValue.a, 1);
        pOut = (SProperty*) pColor;
        break;
    }
    case EPropertyType::Asset: {
         TAssetProperty *pAsset = new TAssetProperty();
        mpReader->Load(DataAddress, &pAsset->DefaultValue, 8);
        pOut = (SProperty*) pAsset;
        break;
    }
    case EPropertyType::MayaSpline: {
        TSplineProperty *pSpline = new TSplineProperty();
        mpReader->Load(DataAddress + 0x00, &pSpline->DefaultValue.v0x00, 4);
        mpReader->Load(DataAddress + 0x04, &pSpline->DefaultValue.v0x04, 4);
        mpReader->Load(DataAddress + 0x08, &pSpline->DefaultValue.v0x08, 4);
        mpReader->Load(DataAddress + 0x0C, &pSpline->DefaultValue.v0x0C, 4);
        mpReader->Load(DataAddress + 0x10, &pSpline->DefaultValue.v0x10, 4);
        mpReader->Load(DataAddress + 0x14, &pSpline->DefaultValue.v0x14, 1);
        mpReader->Load(DataAddress + 0x15, &pSpline->DefaultValue.v0x15, 1);
        mpReader->Load(DataAddress + 0x16, &pSpline->DefaultValue.v0x16, 1);
        mpReader->Load(DataAddress + 0x17, &pSpline->DefaultValue.v0x17, 1);
        mpReader->Load(DataAddress + 0x18, &pSpline->DefaultValue.v0x18, 2);
        mpReader->Load(DataAddress + 0x1A, &pSpline->DefaultValue.v0x1A, 2);
        mpReader->Load(DataAddress + 0x1C, &pSpline->DefaultValue.v0x1C, 4);
        pOut = (SProperty*) pSpline;
        break;
    }
    case EPropertyType::LayerID: {
        // Layer ID loads in reverse order to account for endianness (it's a 128-bit value)
        TLayerIDProperty *pLayer = new TLayerIDProperty();
        mpReader->Load(DataAddress + 0x00, &pLayer->DefaultValue.LayerID_B, 8);
        mpReader->Load(DataAddress + 0x08, &pLayer->DefaultValue.LayerID_A, 8);
        pOut = (SProperty*) pLayer;
        break;
    }
    case EPropertyType::LayerSwitch: {
        TLayerSwitchProperty *pLayer = new TLayerSwitchProperty();
        mpReader->Load(DataAddress + 0x00, &pLayer->DefaultValue.AreaID, 8);
        mpReader->Load(DataAddress + 0x08, &pLayer->DefaultValue.LayerID.LayerID_B, 8);
        mpReader->Load(DataAddress + 0x10, &pLayer->DefaultValue.LayerID.LayerID_A, 8);
        pOut = (SProperty*) pLayer;
        break;
    }
    case EPropertyType::Character: {
        TCharacterProperty *pChar = new TCharacterProperty();
        mpReader->Load(DataAddress + 0x00, &pChar->DefaultValue.AssetID, 8);
        mpReader->Load(DataAddress + 0x08, &pChar->DefaultValue.v0x08, 4);
        mpReader->Load(DataAddress + 0x0C, &pChar->DefaultValue.v0x0C, 4);
        mpReader->Load(DataAddress + 0x10, &pChar->DefaultValue.v0x10, 4);
        pOut = (SProperty*) pChar;
        break;
    }
    case EPropertyType::Transform: {
        TTransformProperty *pXform = new TTransformProperty();
        mpReader->Load(DataAddress + 0x00, &pXform->DefaultValue.Position.x, 4);
        mpReader->Load(DataAddress + 0x04, &pXform->DefaultValue.Position.y, 4);
        mpReader->Load(DataAddress + 0x08, &pXform->DefaultValue.Position.z, 4);
        mpReader->Load(DataAddress + 0x0C, &pXform->DefaultValue.Rotation.x, 4);
        mpReader->Load(DataAddress + 0x10, &pXform->DefaultValue.Rotation.y, 4);
        mpReader->Load(DataAddress + 0x14, &pXform->DefaultValue.Rotation.z, 4);
        mpReader->Load(DataAddress + 0x18, &pXform->DefaultValue.Scale.x, 4);
        mpReader->Load(DataAddress + 0x1C, &pXform->DefaultValue.Scale.y, 4);
        mpReader->Load(DataAddress + 0x20, &pXform->DefaultValue.Scale.z, 4);
        pOut = (SProperty*) pXform;
        break;
    }
    case EPropertyType::Array: {
        TArrayProperty *pArray = new TArrayProperty();
        mpReader->Load(DataAddress + 0x0, &pArray->DefaultValue.v0x0, 4);
        mpReader->Load(DataAddress + 0x4, &pArray->DefaultValue.v0x4, 4);
        mpReader->Load(DataAddress + 0x8, &pArray->DefaultValue.v0x8, 4);
        pOut = (SProperty*) pArray;
        break;
    }
    case EPropertyType::Enum:
    case EPropertyType::Struct:
        errorf("Don't call MakeProperty for enum or struct properties");
        break;
    }

    pOut->DataAddress = pBase->DataAddress;
    pOut->ID = pBase->ID;
    pOut->LoaderAddress = pBase->LoaderAddress;
    pOut->Type = Type;

    pStruct->SubProperties.removeOne(pBase);
    pStruct->SubProperties.append(pOut);
    delete pBase;

    return pOut;
}
