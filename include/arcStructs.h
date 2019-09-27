#include "switch/types.h"
#pragma pack(push, 1)

struct _sArcHeader
{
  u64 Magic;
  s64 MusicDataOffset;
  s64 FileDataOffset;
  s64 FileDataOffset2;
  s64 FileSystemOffset;
  s64 FileSystemSearchOffset;
  s64 Padding;
};

struct _sCompressedTableHeader
{
  u32 DataOffset;
  s32 DecompressedSize;
  s32 CompressedSize;
  s32 SectionSize;
};

struct _sDirectoryList
{
  u32 FullPathHash;
  u32 FullPathHashLengthAndIndex; // index to 0x922D

  u32 NameHash;
  u32 NameHashLength;

  u32 ParentFolderHash;
  u32 ParentFolderHashLength;

  u32 ExtraDisRe; // disposible and resident only 4 entries have this
  u32 ExtraDisReLength;

  s32 FileInformationStartIndex;
  s32 FileInformationCount;

  s32 ChildDirectoryStartIndex;
  s32 ChildDirectoryCount;

  u32 Flags; // TODO
};
struct _sDirectoryOffset
{
  s64 Offset;
  u32 UnknownSomeSize; // TODO: decompsize?
  u32 Size;
  u32 SubDataStartIndex;
  u32 SubDataCount;
  u32 RedirectIndex;
};

struct _sFileInformationPath
{
  u32 Path;
  u32 DirectoryIndex;
  u32 Extension;
  u32 FileTableFlag;

  u32 Parent;
  u32 Unk5;
  u32 FileName;
  u32 Unk6;
};
struct _sFileInformationUnknownTable
{
  u32 SomeIndex;
  u32 SomeIndex2;
};
struct _sFileInformationIndex
{
  u32 DirectoryOffsetIndex;
  u32 FileInformationIndex;
};
struct _sFileInformationSubIndex
{
  u32 DirectoryOffsetIndex;
  u32 SubFileIndex;
  u32 FileInformationIndexAndFlag; // TODO figure out flag
};

struct _sRegionalInfo
{
  char unk[0xE];
};

struct _sFileInformationV1
{
    u32 Path;
    u32 DirectoryIndex;
    u32 Extension;
    u32 FileTableFlag;

    u32 Parent;
    u32 Unk5;
    u32 Hash2;
    u32 Unk6;

    u32 SubFile_Index;
    u32 Flags;
};

struct _sFileInformationV2
{
  u32 PathIndex;
  u32 IndexIndex;

  u32 SubIndexIndex;
  u32 Flags;
};

struct _sFileSystemHeaderV1
{
    u32 FileSize;
    u32 FolderCount;
    u32 FileCount1;
    u32 FileNameCount;

    u32 SubFileCount;
    u32 LastTableCount;
    u32 HashFolderCount;
    u32 FileInformationCount;

    u32 FileCount2;
    u32 SubFileCount2;
    u32 unk1_10;
    u32 unk2_10;

    u8 AnotherHashTableSize;
    u8 unk11;
    u16 unk12;

    u32 MovieCount;
    u32 Part1Count;
    u32 Part2Count;
    u32 MusicFileCount;
};

struct _sFileSystemHeader
{
  u32 TableFileSize;
  u32 FileInformationPathCount;
  u32 FileInformationIndexCount;
  u32 DirectoryCount;

  u32 DirectoryOffsetCount1;

  u32 DirectoryHashSearchCount;
  u32 FileInformationCount;
  u32 FileInformationSubIndexCount;
  u32 SubFileCount;

  u32 DirectoryOffsetCount2;
  u32 SubFileCount2;
  u32 padding; // padding

  u32 unk1_10; // both always 0x10
  u32 unk2_10;
  unsigned char RegionalCount1; // 0xE
  unsigned char RegionalCount2; // 0x5
  u16 padding2;
};
struct _sStreamHeader
{
  u32 UnkCount;
  u32 StreamHashCount;
  u32 StreamIndexToOffsetCount;
  u32 StreamOffsetCount;
};
struct _sStreamUnk
{
  u32 Hash;
  u32 LengthAndSize;
  u32 Index;
};

struct _sSearchHashHeader
{
  u64 SectionSize;
  u32 FolderLengthHashCount;
  u32 SomeCount3;
  u32 SomeCount4;
};
struct _sHashIndexGroup
{
  u32 Hash;
  s16 index;
};
struct _sHashGroup
{
  u32 FilePathHash;
  s16 FilePathLengthAndIndex;
  u32 FolderHash;
  s16 FolderHashLengthAndIndex;
  u32 FileNameHash;
  s16 FileNameLength;
  u32 ExtensionHash;
  u32 ExtensionLength;
};

struct _sSubFileInfo
{
  u32 Offset;
  u32 CompSize;
  u32 DecompSize;
  u32 Flags; // 0x03 if compressed
};

#pragma pack(pop)

struct _sStreamHashToName
{
  u32 Hash;
  u32 NameIndex;
};
struct _sStreamNameToHash
{
  u32 Hash;
  u32 NameIndex;
  u32 Flags;
};
struct _sStreamIndexToOffset
{
  s16 FileIndex;
};
struct _sStreamOffset
{
  s64 Size;
  s64 Offset;
};