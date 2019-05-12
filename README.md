# RetroPropertyExtractor

This is a utility I wrote in late 2015 to extract property data for script objects from Retro Studios games, for use in Prime World Editor. This was written as a one-off utility and wasn't meant to be maintained over a long period of time, and so it is probably not particularly flexible or easy to use/modify, but the code is here just in case it proves to be useful again at some point in the future!

Compiles in Qt Creator with MSVC 2017, other platforms/compilers untested

# Background

In Retro games, starting from Metroid Prime 2, every property is associated with a default value and an ID (a CRC32 hash of the property name/type). This allows the data to be read in a non-linear order. The ID is parsed first and the game uses that to determine which variable to load a property value into. If a given ID is unrecognized, then the game can skip over it entirely; otherwise, if a property is missing, it is simply initialized to the default value. Presumably this was done to make the file format more resilient against changes so that object property layouts could be changed without breaking data compatibility.

For editor use, we needed a list of all properties and their IDs. For Prime 2 and 3, it was sufficient to just analyze the scripting files on disk to gather a list of all possible IDs... however, Donkey Kong Country Returns threw a wrench into things; not only does DKCR have a lot of objects that have insanely large property hierarchies, but as a loading speed optimization, properties are now excluded from the file entirely if their value matches the internal default value! This meant it was no longer possible to get a complete list of properties just from analyzing files, and it also meant that it was now necessary to also have a complete listing of default values, which are hardcoded and can only be found by disassembling the game.

Initially we tried to create this list by hand, but it proved to be a very slow, tedious, and error-prone process; an automatic method it was needed. Hence this utility was born.

While this was originally just meant to be used for DKCR (because it was required), the loader code is written the same way in MP2 and MP3, so a side benefit of automating it was that the same method could be applied to also obtain default property values for the Prime series.

# How it works

The utility works by running a very basic PowerPC interpreter (CByteCodeReader) and using it to analyze the game code.

In each game, each object type is associated with a 4cc ID and a loader function. There is a function that builds a table with each ID and a function pointer to the loader. We execute this function to compile a list of objects and then iterate over the table and analyze each loader function.

Retro created the loader functions via auto-generated C++ code, which means the structure of each function is very similar. The game starts by creating a struct to read the properties into; the constructor initializes every field to its default value. Then it starts parsing the file scripting data from an input stream, reading in the ID, then branches to read the value based on what the ID is. The branch might do something as simple as reading an int from the input stream, or it might branch to another loader function entirely to load another struct.

When we analyze this code (IPropertyLoader), we supply it some memory and execute the constructor in its entirety, which loads all property default values into memory. When we detect we're done with initialization, we start analyzing and selectively executing instructions to gather a full list of property IDs and the corresponding locations that each ID branches to. Then we iterate over each code path and analyze it (without executing it) to determine the type of the property and what address it writes to. Once we have that info, we can fetch the data stored at that address to pull out the default value and associate it with the property.

Once a complete listing of properties is built, it gets dumped to Prime World Editor's template XML format (CTemplateWriter).

There are a lot of common structs that are used by more than one object type (such as EditorProperties, ActorParameters, AudioPlaybackParms). The utility generates a listing of all unique structs and their loader/constructor addresses and dumps them out alongside the script object templates. However it's important to keep in mind that in a lot of cases, objects will actually override the default values of these structs with different data. The utility does account for this and is able to detect these overrides and include them in the dumped XML data.

Other notes:

* There's some small differences in the structures of different loader functions, largely varying based on compiler optimizations like inlining. A lot of one-off differences are accounted for by checking against hardcoded addresses in IPropertyLoader subclasses.
* A lot of loader functions are not part of the main executable but are instead stored in relocatable modules (REL/RSO files). All REL/RSOs are loaded and linked in memory before any analysis is done (CDynamicLinker).
* The Metroid Prime 2 NTSC demo has duplicate RELs with both production and non-production versions present. The production ones are suffixed with 'P' and are the only ones that actually link into the game correctly. The non-production versions are ignored.
* The Metroid Prime 3 E3 demo prototype build contains a lot of debug strings that other game builds don't have. Every loader prints an error message when an unrecognized ID is encountered. When running for this build, we also parse the debug string in order to dump out the name of the struct/object.
* If you have generated templates for the Metroid Prime 3 prototype, the utility can reparse those templates and remap struct/enum names into other games by comparing against shared property IDs.
* Some property types (particularly choice/sound/asset (in MP2)/animation ID (in DKCR)) aren't possible to distinguish from int properties from the code, since the same bytecode is generated for all of these types. These are all dumped as int properties even though that technically isn't accurate.
* Metroid Prime 3 and DKCR have enum properties where the value written to the file is a CRC32 hash of the value's name in the source code. When the game reads these, it looks up the hash in a table mapping each hash to the real enum's internal value. The utility detects this and parses the table to dump out a list of all possible values for the enum.
* Properties for ScannableObjectInfo (SCAN assets) and Tweaks can be dumped as well, but since they aren't script objects, there are some special caveats. They are not part of the script object table. Tweaks are also global objects and so the constructor that populates the default values isn't called from the property loader. Addresses are hardcoded to get around this.
* There seem to be a couple enum properties in some games that have invalid default values, causing the tool to spit out errors.