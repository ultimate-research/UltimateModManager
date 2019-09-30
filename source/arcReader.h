#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <streambuf>
#include <vector>
#include "arcStructs.h"
#include "crc32.h"

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#define LOADARRAY(arr, type, count)              \
    arr = (type*)malloc(sizeof(type) * (count)); \
    for (size_t i = 0; i < count; i++) reader.load(arr[i])

struct membuf : std::streambuf {
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};

class ObjectStream {
   public:
    std::istream& is;
    u64 pos;
    ObjectStream(std::istream& _is) : is(_is) { pos = 0; };
    template <class T>
    void load(T& v) {
        is.read((char*)&v, sizeof(v));
        pos += sizeof(v);
    }
};

class ArcReader {
   private:
    std::map<u32, _sFileInformationV2> pathToFileInfo;
    std::map<u32, _sFileInformationV1> pathToFileInfoV1;
    std::map<u32, _sStreamNameToHash> pathCrc32ToStreamInfo;
    std::vector<u32> FilePaths;
    u64 numFiles;
    ZSTD_DStream* dstream;
    std::string arcPath;

    const u64 Magic = 0xABCDEF9876543210;

    _sArcHeader header;

    // stream
    _sStreamUnk* streamUnk;
    _sStreamHashToName* streamHashToName;
    _sStreamNameToHash* streamNameToHash;
    _sStreamIndexToOffset* streamIndexToFile;
    _sStreamOffset* streamOffsets;

    // file system
    _sFileSystemHeader fsHeader;
    _sRegionalInfo* regionalInfo;
    _sStreamHeader streamHeader;

    _sFileInformationPath* fileInfoPath;
    _sFileInformationIndex* fileInfoIndex;
    _sFileInformationSubIndex* fileInfoSubIndex;
    _sFileInformationV2* fileInfoV2;

    _sSubFileInfo* subFiles;

    _sFileInformationUnknownTable* fileInfoUnknownTable;
    _sHashIndexGroup* filePathToIndexHashGroup;

    // Directory information
    _sHashIndexGroup* directoryHashGroup;
    _sDirectoryList* directoryList;
    _sDirectoryOffset* directoryOffsets;
    _sHashIndexGroup* directoryChildHashGroup;

    // V1 arc only
    _sFileInformationV1* fileInfoV1;

    bool zstd_decompress(void* comp, void* decomp, uint32_t comp_size, uint32_t decomp_size) {
        ZSTD_resetDStream(dstream);

        ZSTD_inBuffer input = {comp, comp_size, 0};
        ZSTD_outBuffer output = {decomp, decomp_size, 0};

        size_t ret = ZSTD_decompressStream(dstream, &output, &input);
        if (ZSTD_isError(ret)) {
            printf("err %s\n", ZSTD_getErrorName(ret));
            return false;
        }

        return true;
    }

    bool Init() {
        dstream = ZSTD_createDStream();
        ZSTD_initDStream(dstream);
        std::fstream reader(this->arcPath);
        reader.read((char*)&header, sizeof(header));
        if (header.Magic != Magic) {
            printf("ARC magic does not match\n");  // add proper errors
            return false;
        }
        reader.seekg(header.FileSystemOffset);

        s32 tableSize;
        char* table;
        table = ReadCompressedTable(reader, &tableSize);
        if (header.FileDataOffset < 0x8824AF68) {
            Version = 0x00010000;
            if (table) {
                ReadFileSystemV1(table, tableSize);
                free(table);
            }
        } else {
            if (table) {
                ReadFileSystem(table, tableSize);
                free(table);
            }
        }
        reader.close();
        ZSTD_freeDStream(dstream);
        return true;
    }

    void ReadFileSystem(char* table, s32 tableSize) {
        membuf sbuf(table, table + tableSize);
        std::istream iTableReader(&sbuf);
        ObjectStream reader(iTableReader);

        _sFileSystemHeader fsHeader;
        reader.load(fsHeader);

        u32 extraFolder = 0;
        u32 extraCount = 0;
        u32 extraCount2 = 0;
        u32 extraSubCount = 0;

        if (tableSize >= 0x2992DD4) {
            // Version 3+
            reader.load(Version);

            reader.load(extraFolder);
            reader.load(extraCount);

            u64 junk;
            reader.load(junk);  // some extra thing :thinking
            reader.load(extraCount2);
            reader.load(extraSubCount);
        } else {
            Version = 0x00020000;
            reader.is.seekg(0x3C, reader.is.beg);
        }

        // skip this for now
        LOADARRAY(regionalInfo, _sRegionalInfo, 12);

        // Streams
        reader.load(streamHeader);

        LOADARRAY(streamUnk, _sStreamUnk, streamHeader.UnkCount);
        LOADARRAY(streamHashToName, _sStreamHashToName, streamHeader.StreamHashCount);
        LOADARRAY(streamNameToHash, _sStreamNameToHash, streamHeader.StreamHashCount);
        LOADARRAY(streamIndexToFile, _sStreamIndexToOffset, streamHeader.StreamIndexToOffsetCount);
        LOADARRAY(streamOffsets, _sStreamOffset, streamHeader.StreamOffsetCount);

        // Unknown
        u32 unkCount1, unkCount2;
        reader.load(unkCount1);
        reader.load(unkCount2);

        LOADARRAY(fileInfoUnknownTable, _sFileInformationUnknownTable, unkCount2);
        LOADARRAY(filePathToIndexHashGroup, _sHashIndexGroup, unkCount1);

        // FileTables

        LOADARRAY(fileInfoPath, _sFileInformationPath, fsHeader.FileInformationPathCount);
        LOADARRAY(fileInfoIndex, _sFileInformationIndex, fsHeader.FileInformationIndexCount);

        // directory tables

        // directory hashes by length and index to directory probably 0x6000 something
        LOADARRAY(directoryHashGroup, _sHashIndexGroup, fsHeader.DirectoryCount);
        LOADARRAY(directoryList, _sDirectoryList, fsHeader.DirectoryCount);
        LOADARRAY(directoryOffsets, _sDirectoryOffset, fsHeader.DirectoryOffsetCount1 + fsHeader.DirectoryOffsetCount2 + extraFolder);
        LOADARRAY(directoryChildHashGroup, _sHashIndexGroup, fsHeader.DirectoryHashSearchCount);

        // file information tables
        numFiles = fsHeader.FileInformationCount + fsHeader.SubFileCount2 + extraCount;
        LOADARRAY(fileInfoV2, _sFileInformationV2, numFiles);
        LOADARRAY(fileInfoSubIndex, _sFileInformationSubIndex, fsHeader.FileInformationSubIndexCount + fsHeader.SubFileCount2 + extraCount2);
        LOADARRAY(subFiles, _sSubFileInfo, fsHeader.SubFileCount + fsHeader.SubFileCount2 + extraSubCount);
    }

    void ReadFileSystemV1(char* table, s32 tableSize) {
        membuf sbuf(table, table + tableSize);
        std::istream iTableReader(&sbuf);
        ObjectStream reader(iTableReader);

        _sFileSystemHeaderV1 nodeHeader;
        reader.load(nodeHeader);

        reader.is.seekg(0x68, reader.is.beg);

        // Hash Table
        reader.is.seekg(0x8 * nodeHeader.Part1Count);

        // Hash Table 2
        LOADARRAY(streamNameToHash, _sStreamNameToHash, nodeHeader.Part1Count);

        // Hash Table 3
        LOADARRAY(streamIndexToFile, _sStreamIndexToOffset, nodeHeader.Part2Count);

        // stream offsets
        LOADARRAY(streamOffsets, _sStreamOffset, nodeHeader.MusicFileCount);

        // Another Hash Table
        reader.is.seekg(0xC * 0xE);

        //folders
        LOADARRAY(directoryList, _sDirectoryList, nodeHeader.FolderCount);

        //file offsets

        LOADARRAY(directoryOffsets, _sDirectoryOffset, nodeHeader.FileCount1 + nodeHeader.FileCount2);
        //DirectoryOffsets_2 = reader.ReadType<_sDirectoryOffsets>(R, NodeHeader.FileCount2);
        LOADARRAY(directoryChildHashGroup, _sHashIndexGroup, nodeHeader.HashFolderCount);
        LOADARRAY(fileInfoV1, _sFileInformationV1, nodeHeader.FileInformationCount);
        LOADARRAY(subFiles, _sSubFileInfo, nodeHeader.SubFileCount + nodeHeader.SubFileCount2);
    }

    char* ReadCompressedTable(std::fstream& reader, s32* tableSize) {
        _sCompressedTableHeader compHeader;
        reader.read((char*)&compHeader, sizeof(compHeader));

        char* tableBytes;
        s32 size;

        if (compHeader.DataOffset > 0x10) {
            u64 currPos = reader.tellg();
            u64 tableStart = currPos - 0x10;
            reader.seekg(tableStart, reader.beg);
            reader.read((char*)&size, sizeof(s32));
            reader.seekg(tableStart, reader.beg);
            tableBytes = (char*)malloc(size);
            reader.read(tableBytes, size);
        } else if (compHeader.DataOffset == 0x10) {
            char* compressedTableBytes = (char*)malloc(compHeader.CompressedSize);
            reader.read(compressedTableBytes, compHeader.CompressedSize);

            size = compHeader.DecompressedSize;
            tableBytes = (char*)malloc(size);
            zstd_decompress(compressedTableBytes, tableBytes, compHeader.CompressedSize, compHeader.DecompressedSize);
            free(compressedTableBytes);
        } else {
            size = compHeader.CompressedSize;
            tableBytes = (char*)malloc(size);
            reader.read(tableBytes, size);
        }

        *tableSize = size;
        return tableBytes;
    }

    void Deinit() {
        free(regionalInfo);

        // Streams
        free(streamUnk);
        free(streamHashToName);
        free(streamNameToHash);
        free(streamIndexToFile);
        free(streamOffsets);

        // Unknown
        free(fileInfoUnknownTable);
        free(filePathToIndexHashGroup);

        // FileTables
        free(fileInfoPath);
        free(fileInfoIndex);

        // dir info
        free(directoryHashGroup);
        free(directoryList);
        free(directoryOffsets);
        free(directoryChildHashGroup);

        // file information tables
        free(fileInfoV2);
        free(fileInfoSubIndex);
        free(subFiles);
    }

   public:
    static const size_t NUM_REGIONS = 14;
    std::string RegionTags[NUM_REGIONS] =
    {
        "+jp_ja",
        "+us_en",
        "+us_fr",
        "+us_es",
        "+eu_en",
        "+eu_fr",
        "+eu_es",
        "+eu_de",
        "+eu_nl",
        "+eu_it",
        "+eu_ru",
        "+kr_ko",
        "+zh_cn",
        "+zh_tw"
    };

    void GetFileInformation(std::string arcFileName, long& offset, u32& compSize, u32& decompSize, bool& regional, int regionIndex = 1) {
        size_t semicolonIndex;
        if ((semicolonIndex = arcFileName.find(";")) != std::string::npos)
            arcFileName[semicolonIndex] = ':';

        for (size_t i = 0; i < NUM_REGIONS; i++) {
            std::string regionTag = RegionTags[i];
            size_t pos;
            if ((pos = arcFileName.find(regionTag)) != std::string::npos) {
                arcFileName.erase(pos, regionTag.size());
                regionIndex = i;
                break;
            }
        }

        u32 path_hash = crc32(arcFileName.c_str(), arcFileName.size());

        offset = 0;
        compSize = 0;
        decompSize = 0;
        regional = false;

        if ((Version != 0x00010000 && pathToFileInfo.count(path_hash) == 0) ||
            (Version == 0x00010000 && pathToFileInfoV1.count(path_hash) == 0)) {  //
            // check for stream file
            if (pathCrc32ToStreamInfo.count(path_hash) != 0) {
                auto fileinfo = pathCrc32ToStreamInfo[path_hash];

                if (fileinfo.Flags == 1 || fileinfo.Flags == 2) {
                    if (fileinfo.Flags == 2 && regionIndex > 5)
                        regionIndex = 0;

                    auto streamindex = streamIndexToFile[(fileinfo.NameIndex >> 8) + regionIndex].FileIndex;
                    auto offsetinfo = streamOffsets[streamindex];
                    offset = offsetinfo.Offset;
                    compSize = (u32)offsetinfo.Size;
                    decompSize = (u32)offsetinfo.Size;
                    regional = true;
                } else {
                    auto streamindex = streamIndexToFile[fileinfo.NameIndex >> 8].FileIndex;
                    auto offsetinfo = streamOffsets[streamindex];
                    offset = offsetinfo.Offset;
                    compSize = (u32)offsetinfo.Size;
                    decompSize = (u32)offsetinfo.Size;
                }
                return;
            }

            return;
        }
        if (IsRegional(path_hash))
            regional = true;

        if (Version == 0x00010000)
            GetFileInformation(pathToFileInfoV1[path_hash], offset, compSize, decompSize, regionIndex);
        else
            GetFileInformation(pathToFileInfo[path_hash], offset, compSize, decompSize, regionIndex);
    }

    void GetFileInformation(_sFileInformationV2 fileinfo, long& offset, u32& compSize, u32& decompSize, int regionIndex) {
        auto fileIndex = fileInfoIndex[fileinfo.IndexIndex];

        //redirect
        if ((fileinfo.Flags & 0x00000010) == 0x10) {
            fileinfo = fileInfoV2[fileIndex.FileInformationIndex];
        }

        auto subIndex = fileInfoSubIndex[fileinfo.SubIndexIndex];

        auto subFile = subFiles[subIndex.SubFileIndex];
        auto directoryOffset = directoryOffsets[subIndex.DirectoryOffsetIndex];

        //regional
        if ((fileinfo.Flags & 0x00008000) == 0x8000) {
            subIndex = fileInfoSubIndex[fileinfo.SubIndexIndex + 1 + regionIndex];
            subFile = subFiles[subIndex.SubFileIndex];
            directoryOffset = directoryOffsets[subIndex.DirectoryOffsetIndex];
        }

        offset = (header.FileDataOffset + directoryOffset.Offset + (subFile.Offset << 2));
        compSize = subFile.CompSize;
        decompSize = subFile.DecompSize;
    }

    void GetFileInformation(_sFileInformationV1 fileInfo, long& offset, u32& compSize, u32& decompSize, int regionIndex) {
        auto subFile = subFiles[fileInfo.SubFile_Index];
        auto dirIndex = directoryList[fileInfo.DirectoryIndex >> 8].FullPathHashLengthAndIndex >> 8;
        auto directoryOffset = directoryOffsets[dirIndex];

        //redirect
        if ((fileInfo.Flags & 0x00300000) == 0x00300000)
        {
            GetFileInformation(fileInfoV1[subFile.Flags&0xFFFFFF], offset, compSize, decompSize, regionIndex);
            return;
        }

        //regional
        if ((fileInfo.FileTableFlag >> 8) > 0)
        {
            subFile = subFiles[(fileInfo.FileTableFlag >> 8) + regionIndex];
            directoryOffset = directoryOffsets[dirIndex + 1 + regionIndex];
        }

        offset = (header.FileDataOffset + directoryOffset.Offset + (subFile.Offset << 2));
        compSize = subFile.CompSize;
        decompSize = subFile.DecompSize;
    }

    bool IsRedirected(u32 path_hash) {
        if (pathToFileInfoV1.count(path_hash) != 0)
            return ((pathToFileInfoV1[path_hash].Flags & 0x00300000) == 0x00300000);
        if (pathToFileInfo.count(path_hash) != 0)
            return (pathToFileInfo[path_hash].Flags & 0x00000010) == 0x10;
        return false;
    }

    bool IsRegional(u32 path_hash) {
        if (pathToFileInfoV1.count(path_hash) != 0)
            return ((pathToFileInfoV1[path_hash].FileTableFlag >> 8) > 0);
        if (pathToFileInfo.count(path_hash) != 0)
            return ((pathToFileInfo[path_hash].Flags & 0x00008000) == 0x8000);
        return false;
    }

    void InitializePathToFileInfo() {
        for (size_t i = 0; i < streamHeader.StreamHashCount; i++) {
            auto v = streamNameToHash[i];
            pathCrc32ToStreamInfo.try_emplace(v.Hash, v);
        }

        if (Version == 0x00010000) {
            for (size_t i = 0; i < FilePaths.size(); i++)
            {
                if (!pathToFileInfoV1.count(FilePaths[i]) == 0)
                    pathToFileInfoV1.try_emplace(FilePaths[i], fileInfoV1[i]);
            }
        } else {
            for (size_t i = 0; i < FilePaths.size(); i++) {
                if (pathToFileInfo.count(FilePaths[i]) == 0)
                    pathToFileInfo.try_emplace(FilePaths[i], fileInfoV2[i]);
            }
        }
    }

    std::vector<u32> GetFileList() {
        if (Version == 0x00010000)
           return GetFileListV1();
        else
            return GetFileListV2();
    }

    std::vector<u32> GetFileListV1() {
        std::vector<u32> filePaths;

        for (size_t i = 0; i < numFiles; i++) {
            auto fileInfo = fileInfoV1[i];
            filePaths.push_back(fileInfo.Path);
        }

        return filePaths;
    }

    std::vector<u32> GetFileListV2() {
        std::vector<u32> filePaths;

        for (size_t i = 0; i < numFiles; i++) {
            auto fileInfo = fileInfoV2[i];
            auto path = fileInfoPath[fileInfo.PathIndex];
            filePaths.push_back(path.Path);
        }

        return filePaths;
    }

    int Version;
    ArcReader(std::string arcPath) {
        this->arcPath = arcPath;

        bool ret = Init();
        if (!ret) return;
        FilePaths = GetFileList();
        InitializePathToFileInfo();
    }

    ~ArcReader() {
        Deinit();
    }
};