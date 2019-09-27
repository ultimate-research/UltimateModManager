#include <fstream>
#include <iostream>
#include <istream>
#include <streambuf>
#include "arcStructs.h"

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#define LOADARRAY(arr,type,count) arr = (type *) malloc(sizeof(type) * count); for (size_t i = 0; i < count; i++) reader.load(arr[i])

struct membuf : std::streambuf
{
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }

    void setptrs(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};

template< class T >
T load(std::istream& is) {
    T v;
    is.read((char*)&v, sizeof(v));
    return v;
}

class ObjectStream
{
public:
  std::istream& is;
  u64 pos;
  ObjectStream(std::istream& _is) : is(_is) { pos = 0; };
  template <class T>
  ObjectStream& operator>>(T& v) {
    is.read((char*)&v, sizeof(T));
    return *this;
  }
    template< class T >
    void load( T& v ) {
        is.read((char*)&v, sizeof(v));
        pos += sizeof(v);
    }
};

class arcReader
{
private:
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

    bool zstd_decompress(void* comp, void* decomp, uint32_t comp_size, uint32_t decomp_size)
    {
        ZSTD_resetDStream(dstream);

        ZSTD_inBuffer input = {comp, comp_size, 0};
        ZSTD_outBuffer output = {decomp, decomp_size, 0};

        size_t ret = ZSTD_decompressStream(dstream, &output, &input);
        if (ZSTD_isError(ret))
        {
            printf("err %s\n", ZSTD_getErrorName(ret));
            return false;
        }
        
        return true;
    }

  void Init()
  {
    dstream = ZSTD_createDStream();
    ZSTD_initDStream(dstream);
    std::fstream reader(this->arcPath);
    reader.read((char*)&header, sizeof(header));
    if (header.Magic != Magic) {
      printf("ARC magic does not match");  // add proper errors
      return;
    }
    printf("Magic: 0x%lx\n", header.Magic);
    printf("FileSystemOffset: 0x%lx\n", header.FileSystemOffset);
    reader.seekg(header.FileSystemOffset);
    if (header.FileDataOffset < 0x8824AF68)
    {
      Version = 0x00010000;
      //ReadFileSystemV1(ReadCompressedTable(reader));
    }
    else
    {
      printf("Version > 1.0.0\n");
      s32 tableSize;
      char* table = ReadCompressedTable(reader, &tableSize);

      if (table) {
        ReadFileSystem(table, tableSize);
      }

      free(table);
    }
    reader.close();
    ZSTD_freeDStream(dstream);
  }

  void ReadFileSystem(char* table, s32 tableSize)
    {
        membuf sbuf(table, table + tableSize);
        std::istream iTableReader(&sbuf);
        ObjectStream reader(iTableReader);

        _sFileSystemHeader fsHeader;
        reader.load(fsHeader);

        u32 extraFolder = 0;
        u32 extraCount = 0;
        u32 extraCount2 = 0;
        u32 extraSubCount = 0;

        if (tableSize >= 0x2992DD4)
        {
            // Version 3+
            reader.load(Version);

            reader.load(extraFolder); 
            reader.load(extraCount);

            u64 junk;
            reader.load(junk);  // some extra thing :thinking
            reader.load(extraCount2);
            reader.load(extraSubCount);
        }
        else
        {
            Version = 0x00020000;
            reader.is.seekg(0x3C, reader.is.beg);
        }

        printf("TableFileSize: %d\n", fsHeader.TableFileSize);
        printf("FileInformationPathCount: %d\n", fsHeader.FileInformationPathCount);
        printf("FileInformationIndexCount: %d\n", fsHeader.FileInformationIndexCount);
        printf("DirectoryCount: %d\n", fsHeader.DirectoryCount);

        printf("extraFolder: %d\n", extraFolder);
        printf("extraCount: %d\n", extraCount);
        printf("extraCount2: %d\n", extraCount2);
        printf("extraSubCount: %d\n", extraSubCount);

        printf("Version: %lx\n", (u64)Version);

        // skip this for now
        LOADARRAY(regionalInfo, _sRegionalInfo, 12);

        // Streams
        reader.load(streamHeader);

        printf("streamHeader.UnkCount: %d\n", streamHeader.UnkCount);
        printf("streamHeader.StreamHashCount: %d\n", streamHeader.StreamHashCount);
        printf("streamHeader.StreamHashCount: %d\n", streamHeader.StreamHashCount);
        printf("streamHeader.StreamIndexToOffsetCount: %d\n", streamHeader.StreamIndexToOffsetCount);
        printf("streamHeader.StreamOffsetCount: %d\n", streamHeader.StreamOffsetCount);

        LOADARRAY(streamUnk, _sStreamUnk, streamHeader.UnkCount);
        LOADARRAY(streamHashToName, _sStreamHashToName, streamHeader.StreamHashCount);
        LOADARRAY(streamNameToHash, _sStreamNameToHash, streamHeader.StreamHashCount);
        LOADARRAY(streamIndexToFile, _sStreamIndexToOffset, streamHeader.StreamIndexToOffsetCount);
        LOADARRAY(streamOffsets, _sStreamOffset, streamHeader.StreamOffsetCount);

        printf("%d\n", (s32) reader.pos);

        // Unknown
        u32 unkCount1, unkCount2;
        reader.load(unkCount1);
        reader.load(unkCount2);

        printf("UnkCount1: %d\n", unkCount1);
        printf("UnkCount2: %d\n", unkCount2);
        return;

        LOADARRAY(fileInfoUnknownTable, _sFileInformationUnknownTable, unkCount2);
        LOADARRAY(filePathToIndexHashGroup, _sHashIndexGroup, unkCount1);
        return;

        // FileTables
        
        LOADARRAY(fileInfoPath, _sFileInformationPath, fsHeader.FileInformationPathCount);
        LOADARRAY(fileInfoIndex, _sFileInformationIndex, fsHeader.FileInformationIndexCount);

        // directory tables

        // directory hashes by length and index to directory probably 0x6000 something
        /*Console.WriteLine(reader.BaseStream.Position.ToString("X"));
        directoryHashGroup = reader.ReadType<_sHashIndexGroup>(fsHeader.DirectoryCount);
        
        directoryList = reader.ReadType<_sDirectoryList>(fsHeader.DirectoryCount);
        
        directoryOffsets = reader.ReadType<_sDirectoryOffset>(fsHeader.DirectoryOffsetCount1 + fsHeader.DirectoryOffsetCount2 + extraFolder);
        
        directoryChildHashGroup = reader.ReadType<_sHashIndexGroup>(fsHeader.DirectoryHashSearchCount);

        // file information tables
        Console.WriteLine(reader.BaseStream.Position.ToString("X"));
        fileInfoV2 = reader.ReadType<_sFileInformationV2>(fsHeader.FileInformationCount + fsHeader.SubFileCount2 + extraCount);
        
        fileInfoSubIndex = reader.ReadType<_sFileInformationSubIndex>(fsHeader.FileInformationSubIndexCount + fsHeader.SubFileCount2 + extraCount2);
        
        subFiles = reader.ReadType<_sSubFileInfo>(fsHeader.SubFileCount + fsHeader.SubFileCount2 + extraSubCount);
        Console.WriteLine("End:" + reader.BaseStream.Position.ToString("X"));
        */
    }

    char* ReadCompressedTable(std::fstream& reader, s32* tableSize)
    {
        _sCompressedTableHeader compHeader;
        reader.read((char*)&compHeader, sizeof(compHeader));

        printf("CompHeader: 0x%x, %d, %d, %d\n", compHeader.DataOffset, compHeader.DecompressedSize, compHeader.CompressedSize, compHeader.SectionSize);

        char* tableBytes;
        s32 size;

        if(compHeader.DataOffset > 0x10)
        {
            u64 currPos = reader.tellg();
            u64 tableStart = currPos - 0x10;
            reader.seekg(tableStart, reader.beg);
            reader.read((char*)&size, sizeof(s32));
            reader.seekg(tableStart, reader.beg);
            tableBytes = (char*) malloc(size);
            reader.read(tableBytes, size);
        }
        else
        if(compHeader.DataOffset == 0x10)
        {
            char* compressedTableBytes = (char*) malloc(compHeader.CompressedSize);
            u64 currPos = reader.tellg();
            printf("Reading compressed table at %lx\n", currPos);
            reader.read(compressedTableBytes, compHeader.CompressedSize);

            size = compHeader.DecompressedSize;
            tableBytes = (char*) malloc(size);
            zstd_decompress(compressedTableBytes, tableBytes, compHeader.CompressedSize, compHeader.DecompressedSize);
            free(compressedTableBytes);
        }
        else
        {
            size = compHeader.CompressedSize;
            tableBytes = (char*) malloc(size);
            reader.read(tableBytes, size);
        }

        *tableSize = size;
        return tableBytes;
    }

    void Deinit()
    {
        free(regionalInfo);

        // Streams
        free(streamUnk);
        free(streamHashToName);
        free(streamNameToHash);
        free(streamIndexToFile);
        free(streamOffsets);
        return;

        // Unknown
        free(fileInfoUnknownTable);
        free(filePathToIndexHashGroup);
        return;

        // FileTables
        free(fileInfoPath);
        free(fileInfoIndex);
    }

public:
  int Version;
  bool Initialized;
  arcReader(std::string arcPath)
  {
    this->arcPath = arcPath;
    Init();
    //FilePaths = GetFileList();
    //StreamFilePaths = GetStreamFileList();
    //InitializePathToFileInfo();
  }

  ~arcReader()
  {
      Deinit();
  }
};