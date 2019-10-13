#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <streambuf>
#include <vector>
#include "arcStructs.h"
#include "utils.h"
#include <switch.h>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#define SUBFILE_DECOMPRESSED      0x00000000
#define SUBFILE_COMPRESSION       0x07000000

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

    // extra saved data
    u64 subFilesOffset = 0;
    u32 subFilesCount = 0;
    u64 fileInfoSubIndexOffset = 0;
    u32 fileInfoSubIndexCount = 0;
    u64 dirOffsetsOffset = 0;
    u32 dirOffsetsCount = 0;
    u64 streamOffsetsOffset = 0;
    _sCompressedTableHeader compHeader;
    char* table;

    bool zstd_decompress(void* comp, void* decomp, u32 comp_size, u32 decomp_size) {
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

    size_t zstd_compress(void* comp, size_t comp_size, void* decomp, size_t decomp_size, int cLevel) {
        /*ZSTD_CCtx* const cctx = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel);

        ZSTD_outBuffer output = {comp, comp_size, 0};
        ZSTD_inBuffer input = {decomp, decomp_size, 0};

        size_t ret = ZSTD_compressStream(cctx, &output, &input);
        if (ZSTD_isError(ret)) {
            printf("err %s\n", ZSTD_getErrorName(ret));
            return 0;
        }
        size_t ret2 = ZSTD_endStream(cctx, &output);
        if (ZSTD_isError(ret2)) {
            printf("err %s\n", ZSTD_getErrorName(ret2));
            return 0;
        }
        ZSTD_freeCCtx(cctx);
        return output.pos;*/

        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        ZSTD_parameters params;
        params.fParams = {0,0,1};
        params.cParams = ZSTD_getCParams(cLevel, decomp_size, 0);
        size_t dataSize = ZSTD_compress_advanced(cctx, comp, comp_size, decomp, decomp_size, nullptr, 0, params);
        if(ZSTD_isError(dataSize)) {
            printf("\n\n\n\n\nzstd fail: %s\n", ZSTD_getErrorName(dataSize));
            return 0;
        }
        ZSTD_freeCCtx(cctx);
        return dataSize;
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
        //char* table;
        table = ReadCompressedTable(reader, &tableSize);
        if (header.FileDataOffset < 0x8824AF68) {
            Version = 0x00010000;
            if (table) {
                ReadFileSystemV1(table, tableSize);
                //free(table);
            }
        } else {
            if (table) {
                ReadFileSystem(table, tableSize);
                //free(table);
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
        streamOffsetsOffset = reader.pos;
        LOADARRAY(streamOffsets, _sStreamOffset, streamHeader.StreamOffsetCount);

        // Unknown
        u32 unkCount1, unkCount2;
        reader.load(unkCount1);
        reader.load(unkCount2);

        LOADARRAY(fileInfoUnknownTable, _sFileInformationUnknownTable, unkCount2);
        LOADARRAY(filePathToIndexHashGroup, _sHashIndexGroup, unkCount1);

        // FileTables
        printf("\n\n\nstart: %lx", reader.pos);
        LOADARRAY(fileInfoPath, _sFileInformationPath, fsHeader.FileInformationPathCount);
        printf("\n\n\n\n\nstop: %lx", reader.pos);
        LOADARRAY(fileInfoIndex, _sFileInformationIndex, fsHeader.FileInformationIndexCount);

        // directory tables

        // directory hashes by length and index to directory probably 0x6000 something
        LOADARRAY(directoryHashGroup, _sHashIndexGroup, fsHeader.DirectoryCount);
        LOADARRAY(directoryList, _sDirectoryList, fsHeader.DirectoryCount);
        dirOffsetsOffset = reader.pos;
        dirOffsetsCount = fsHeader.DirectoryOffsetCount1 + fsHeader.DirectoryOffsetCount2 + extraFolder;
        LOADARRAY(directoryOffsets, _sDirectoryOffset, fsHeader.DirectoryOffsetCount1 + fsHeader.DirectoryOffsetCount2 + extraFolder);
        LOADARRAY(directoryChildHashGroup, _sHashIndexGroup, fsHeader.DirectoryHashSearchCount);

        // file information tables
        numFiles = fsHeader.FileInformationCount + fsHeader.SubFileCount2 + extraCount;
        LOADARRAY(fileInfoV2, _sFileInformationV2, numFiles);
        fileInfoSubIndexOffset = reader.pos;
        fileInfoSubIndexCount = fsHeader.FileInformationSubIndexCount + fsHeader.SubFileCount2 + extraCount2;
        LOADARRAY(fileInfoSubIndex, _sFileInformationSubIndex, fsHeader.FileInformationSubIndexCount + fsHeader.SubFileCount2 + extraCount2);
        subFilesOffset = reader.pos;
        subFilesCount = fsHeader.SubFileCount + fsHeader.SubFileCount2 + extraSubCount;
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
        //_sCompressedTableHeader compHeader;
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
            backupTable((char*)&compHeader, sizeof(compHeader), compressedTableBytes, compHeader.CompressedSize);
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

        // extra
        free(table);
    }

    void updateTableData(void* data, u64 totalSize, u64 offset) {
        char* charData = (char*)data;
        for(u64 i = 0; i < totalSize; i++) table[offset+i] = charData[i];
    }
    void writeTableData (FILE * writer) {
        errno = 0;
        if (compHeader.DataOffset > 0x10) printf("\nOOF\n");  // todo
        else if (compHeader.DataOffset == 0x10) {
            size_t origTableSize;
            if(std::filesystem::exists(tablePath))
                origTableSize = std::filesystem::file_size(tablePath);
            else
                origTableSize = compHeader.CompressedSize;
            char* compTable = (char*)malloc(origTableSize);
            size_t compTableSize = zstd_compress(compTable, origTableSize, table, compHeader.DecompressedSize, 22);
            if(compTableSize == 0) {
                printf("compFail\n");
                return;
            }
            compHeader.CompressedSize = compTableSize;
            if(fseek(writer, header.FileSystemOffset, SEEK_SET) != 0) printf("seek: %s\n", strerror(errno));
            size_t ret = fwrite((char*)&compHeader, sizeof(char), sizeof(_sCompressedTableHeader), writer);
            if(ret != sizeof(_sCompressedTableHeader)) {
                printf("write header: %s\n", strerror(errno));
                return;
            }
            ret = fwrite(compTable, sizeof(char), compTableSize, writer);
            if(ret != compTableSize) printf("write: %s\n", strerror(errno));
            fclose(writer);
            free(compTable);
        }
        else printf("\nOOF2\n");  // todo
    }
    void backupTable(char* header, size_t headerSize, char* table, size_t tableSize) {
        if(!std::filesystem::exists(tablePath)) {
            FILE* writer = fopen(tablePath, "wb");
            fwrite(header, sizeof(char), headerSize, writer);
            fwrite(table, sizeof(char), tableSize, writer);
            fclose(writer);
        }
    }

    void checkRegionalSuffix(std::string& path, int& regionIndex) {
        size_t semicolonIndex;
        if ((semicolonIndex = path.find(";")) != std::string::npos)
            path[semicolonIndex] = ':';

        for (size_t i = 0; i < NUM_REGIONS; i++) {
            std::string regionTag = RegionTags[i];
            size_t pos;
            if ((pos = path.find(regionTag)) != std::string::npos) {
                path.erase(pos, regionTag.size());
                regionIndex = i;
                break;
            }
        }
    }

   public:
    int Version;
    u64 fileDataEnd() {
        u64 offset = 0;
        u64 end = 0;
        for(u64 i = 0; i < numFiles; i++) {
            //_sFileInformationPath path = fileInfoPath[fileInfoV2[i].PathIndex];
            _sFileInformationSubIndex subIndex = fileInfoSubIndex[fileInfoV2[i].SubIndexIndex];
            _sSubFileInfo subFile = subFiles[subIndex.SubFileIndex];
            _sDirectoryOffset directoryOffset = directoryOffsets[subIndex.DirectoryOffsetIndex];
            offset = header.FileDataOffset + directoryOffset.Offset + (subFile.Offset<<2) + subFile.CompSize;
            if (offset > end) {
                end = offset;
            }
        }
        return end;
    }
    u32 lastDirOffsetIndex() {
        u32 last = 0;
        for(u64 i = 0; i < numFiles; i++) {
            //_sFileInformationPath path = fileInfoPath[fileInfoV2[i].PathIndex];
            _sFileInformationSubIndex subIndex = fileInfoSubIndex[fileInfoV2[i].SubIndexIndex];
            _sDirectoryOffset directoryOffset = directoryOffsets[subIndex.DirectoryOffsetIndex];
            if (directoryOffset.Offset > directoryOffsets[last].Offset) {
                last = subIndex.DirectoryOffsetIndex;
            }
        }
        return last;
    }
    void writeFileInfo(FILE * arc) {
        updateTableData(directoryOffsets, sizeof(_sDirectoryOffset) * dirOffsetsCount, dirOffsetsOffset);
        updateTableData(streamOffsets, sizeof(_sStreamOffset) * streamHeader.StreamOffsetCount, streamOffsetsOffset);
        updateTableData(fileInfoSubIndex, sizeof(_sFileInformationSubIndex) * fileInfoSubIndexCount, fileInfoSubIndexOffset);
        updateTableData(subFiles, sizeof(_sSubFileInfo) * subFilesCount, subFilesOffset);
        writeTableData(arc);
    }
    bool restoreTable() {
        if(std::filesystem::exists(tablePath)) {
            FILE* reader = fopen(tablePath, "rb");
            size_t fileSize = std::filesystem::file_size(tablePath);
            char* tableBuf = new char[fileSize];
            fread(tableBuf, sizeof(char), fileSize, reader);
            fclose(reader);
            FILE* writer = fopen(this->arcPath.c_str(), "r+b");
            fseek(writer, header.FileSystemOffset, SEEK_SET);
            fwrite(tableBuf, sizeof(char), fileSize, writer);
            fclose(writer);
            Deinit();
            Init();
            return true;
        }
        return false;
    }
    int updateFileInfo(std::string path, u64 Offset = 0, u64 CompSize = 0, u32 DecompSize = 0, u32 Flags = 0) {
        int regionIndex = getRegion();
        checkRegionalSuffix(path, regionIndex);

        u32 path_hash = crc32Calculate(path.c_str(), path.size());
        if ((Version != 0x00010000 && pathToFileInfo.count(path_hash) == 0) ||
            (Version == 0x00010000 && pathToFileInfoV1.count(path_hash) == 0)) {  //
            // check for stream file
            if (pathCrc32ToStreamInfo.count(path_hash) != 0) {
                auto fileinfo = pathCrc32ToStreamInfo[path_hash];
                if (fileinfo.Flags == 1 || fileinfo.Flags == 2) {
                    if (fileinfo.Flags == 2 && regionIndex > 5)
                        regionIndex = 0;
                    auto streamindex = streamIndexToFile[(fileinfo.NameIndex >> 8) + regionIndex].FileIndex;
                    streamOffsets[streamindex].Offset = Offset;
                    streamOffsets[streamindex].Size = CompSize;
                } else {
                    auto streamindex = streamIndexToFile[fileinfo.NameIndex >> 8].FileIndex;
                    streamOffsets[streamindex].Offset = Offset;
                    streamOffsets[streamindex].Size = CompSize;
                }
                return 0;
            }
            return 0;
        }
        if (Version == 0x00010000)
            return -1;
        _sFileInformationV2 fileinfo = pathToFileInfo[path_hash];
        u32 lastdirIDX = lastDirOffsetIndex();
        //u32 origDir = fileInfoSubIndex[fileinfo.SubIndexIndex].DirectoryOffsetIndex;
        u32 subIndexIndex = fileinfo.SubIndexIndex;
        //regional
        if ((fileinfo.Flags & 0x00008000) == 0x8000) {
            subIndexIndex = fileinfo.SubIndexIndex + 1 + regionIndex;
        }
        fileInfoSubIndex[subIndexIndex].DirectoryOffsetIndex = lastdirIDX;
        _sFileInformationSubIndex subIndex = fileInfoSubIndex[subIndexIndex];
        //printf("\n    last:%u\n  actual:%u\noriginal:%u\n", lastdirIDX, subIndex.DirectoryOffsetIndex, origDir);
        //directoryOffsets[subIndex.DirectoryOffsetIndex].UnknownSomeSize += DecompSize;
        //directoryOffsets[subIndex.DirectoryOffsetIndex].SubDataCount++;
        //directoryOffsets[subIndex.DirectoryOffsetIndex].Size += CompSize;
        auto directoryOffset = directoryOffsets[subIndex.DirectoryOffsetIndex];
        //printf("\nFLAGS BEFORE: %x", subFiles[subIndex.SubFileIndex].Flags);
        if((u64)header.FileDataOffset + directoryOffset.Offset + UINT_MAX > Offset) {
            if(Offset != 0) subFiles[subIndex.SubFileIndex].Offset = (Offset - header.FileDataOffset - directoryOffset.Offset) >> 2;
            while(((u64)header.FileDataOffset + directoryOffset.Offset + (subFiles[subIndex.SubFileIndex].Offset << 2)) < Offset)
                subFiles[subIndex.SubFileIndex].Offset++;
            if(CompSize != 0) {
                directoryOffsets[subIndex.DirectoryOffsetIndex].Size -= subFiles[subIndex.SubFileIndex].CompSize;
                directoryOffsets[subIndex.DirectoryOffsetIndex].Size += CompSize;
                subFiles[subIndex.SubFileIndex].CompSize = CompSize;
                if(CompSize == DecompSize) {
                    //subFiles[subIndex.SubFileIndex].Flags &= ~SUBFILE_COMPRESSION;
                    //subFiles[subIndex.SubFileIndex].Flags |= SUBFILE_DECOMPRESSED;
                    subFiles[subIndex.SubFileIndex].Flags = SUBFILE_DECOMPRESSED;
                }
            }
        }
        else printf("\n\nNot close enough\n");
        if(DecompSize != 0) subFiles[subIndex.SubFileIndex].DecompSize = DecompSize;
        if(Flags != 0) subFiles[subIndex.SubFileIndex].Flags = Flags;
        //printf("\nFLAGS AFTER: %x", subFiles[subIndex.SubFileIndex].Flags);
        return 0;
    }
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

    void GetFileInformation(std::string arcFileName, u64& offset, u32& compSize, u32& decompSize, bool& regional, int regionIndex = 1) {
        checkRegionalSuffix(arcFileName, regionIndex);

        u32 path_hash = crc32Calculate(arcFileName.c_str(), arcFileName.size());

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

    void GetFileInformation(_sFileInformationV2 fileinfo, u64& offset, u32& compSize, u32& decompSize, int regionIndex) {
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

    void GetFileInformation(_sFileInformationV1 fileInfo, u64& offset, u32& compSize, u32& decompSize, int regionIndex) {
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