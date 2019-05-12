#include <Common/BasicTypes.h>
#include <Common/FileIO.h>
#include <Common/FileUtil.h>
#include <Common/Log.h>
#include "CByteCodeReader.h"
#include "CTemplateWriter.h"
#include <iostream>

#include "CEchoesDemoLoader.h"
#include "CEchoesLoader.h"
#include "CEchoesTrilogyLoader.h"
#include "CCorruptionProtoLoader.h"
#include "CCorruptionLoader.h"
#include "CDKLoader.h"

#define MP2_HASH            0x797DE7A8
#define MP2_DEMO_HASH       0x09AE263B
#define MP2_TRILOGY_HASH    0xBF490BFA
#define MP3_HASH            0x1AD1FE11
#define MP3_PROTO_HASH      0xE3E4EA70
#define DKCR_HASH           0xD899FA36

int main(int argc, char *argv[])
{
    NLog::InitLog("RetroPropertyExtractor.log");

    // The dol file should be supplied in argv[1]
    // Hash the dol to determine what game this is
    TString DolPath = (argc >= 2 ? argv[1] : "");
    uint32 DolHash = 0xFFFFFFFF;
    std::vector<uint8> DolData;

    if (FileUtil::LoadFileToBuffer(DolPath, DolData))
    {
        DolHash = CCRC32::StaticHashData(DolData.data(), DolData.size());
    }
    else
    {
        errorf("Please provide a path to a DOL file!");
        return 1;
    }

    // Determine game filesystem root
    TString FSTRoot;

    if (DolPath.GetFileName() == "rs5mp2_p.dol")
    {
        FSTRoot = DolPath.GetFileDirectory() / "MP2/";
    }
    else
    {
        // Try to find Standard.ntwk
        // We check for this file because it is in every supported game
        TString DolDir = DolPath.GetFileDirectory();

        if (FileUtil::Exists(DolDir / "Standard.ntwk"))
        {
            FSTRoot = DolDir;
        }
        else if (FileUtil::Exists(DolDir / "../files/Standard.ntwk"))
        {
            FSTRoot = DolDir / "../files/";
        }
        else if (FileUtil::Exists(DolDir / "../Standard.ntwk"))
        {
            FSTRoot = DolDir / "../";
        }
        else
        {
            errorf("Loaded DOL successfully but couldn't find the game filesystem root.");
            return 1;
        }
    }

    FSTRoot = FileUtil::MakeAbsolute(FSTRoot);

    // Create property loader
    CByteCodeReader Reader;
    IPropertyLoader* pLoader = nullptr;

    switch (DolHash)
    {
    case MP2_DEMO_HASH:
        pLoader = new CEchoesDemoLoader(&Reader, DolPath, FSTRoot);
        break;

    case MP2_HASH:
        pLoader = new CEchoesLoader(&Reader, DolPath, FSTRoot);
        break;

    case MP2_TRILOGY_HASH:
        pLoader = new CEchoesTrilogyLoader(&Reader, DolPath, FSTRoot);
        break;

    case MP3_HASH:
        pLoader = new CCorruptionLoader(&Reader, DolPath, FSTRoot);
        break;

    case MP3_PROTO_HASH:
        pLoader = new CCorruptionProtoLoader(&Reader, DolPath, FSTRoot);
        break;

    case DKCR_HASH:
        pLoader = new CDKLoader(&Reader, DolPath, FSTRoot);
        break;

    default:
        warnf("Couldn't recognize the DOL. The following games are supported:\n"
              "    Metroid Prime 2 (NTSC Retail, NTSC Demo Disc, Trilogy)\n"
              "    Metroid Prime 3 (NTSC Retail, E3 2006 Demo Prototype)\n"
              "    Donkey Kong Country Returns (NTSC Retail)");
        return 1;
    }

    // Load dol and execute the function that assigns the script loaders
    CFileInStream Dol(DolPath, EEndian::BigEndian);

    if (!Dol.IsValid())
    {
        errorf("Couldn't open dol");
        return 1;
    }

    pLoader->InitSections();

    Reader.LoadDol(Dol);
    Reader.LoadLinkedObjects(pLoader->LinkedObjectsFolder(), pLoader->HasProductionModules());

    pLoader->InitGlobals();
    pLoader->InitObjects();

#if 0
    // debug memory dump - this is useful for disassembling the game with all modules linked in
    CFileOutStream DumpStream(DolPath.GetFileDirectory() + "memdump.bin", EEndian::BigEndian);
    Reader.DumpMemory(DumpStream);
    DumpStream.Close();
#endif

    pLoader->ParseObjectLoaders();

    CTemplateWriter Writer(pLoader);
    Writer.MakeTemplates();

    delete pLoader;
    return 0;
}

