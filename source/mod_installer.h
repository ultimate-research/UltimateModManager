#pragma once
#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <MainApplication.hpp>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <experimental/filesystem>
#include "utils.h"
#include "offsetFile.h"

#define FILENAME_SIZE 0x130
#define FILE_READ_SIZE 0x20000

#define INSTALL false
#define UNINSTALL true

// Short-term Overrides
#define CONSOLE_BLUE ""
#define CONSOLE_RED ""
#define CONSOLE_GREEN ""
#define CONSOLE_YELLOW ""
#define CONSOLE_RESET ""

bool installing = INSTALL;

char** mod_dirs = NULL;
size_t num_mod_dirs = 0;
bool installation_finish = false;
s64 mod_folder_index = 0;
offsetFile* offsetObj = nullptr;
ZSTD_CCtx* compContext = nullptr;

const char* manager_root = "sdmc:/UltimateModManager/";
const char* mods_root = "sdmc:/UltimateModManager/mods/";
const char* backups_root = "sdmc:/UltimateModManager/backups/";
const char* offsetDBPath = "sdmc:/UltimateModManager/Offsets.txt";

std::shared_ptr<MainApplication> _main;

pu::ui::Color PU_RED = pu::ui::Color(255, 0, 0, 128);
pu::ui::Color PU_GREEN = pu::ui::Color(0, 255, 0, 128);
pu::ui::Color PU_BLUE = pu::ui::Color(0, 0, 255, 128);
pu::ui::Color PU_YELLOW = pu::ui::Color(255, 255, 0, 128);

void _printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[1024];
    vsnprintf(buffer, 1024, fmt, args);

    _main->layout1->helloText->SetText(_main->layout1->helloText->GetText() + buffer);
}

void _clr_overlay(pu::ui::Color clr) {
    auto textBlock = _main->layout1->helloText;
    auto textOverlay = pu::ui::elm::Rectangle::New(0, textBlock->GetY() + textBlock->GetHeight() - 5, 1280, textBlock->singleLineHeight, clr, 0);
    _main->layout1->Add(textOverlay);
    _main->layout1->textOverlays.push_back(textOverlay);
}

int seek_files(FILE* f, uint64_t offset, FILE* arc) {
    // Set file pointers to start of file and offset respectively
    int ret = fseek(f, 0, SEEK_SET);
    if (ret) {
        _clr_overlay(PU_RED);
        _printf("Failed to seek file with errno %d\n", ret);
        return ret;
    }

    ret = fseek(arc, offset, SEEK_SET);
    if (ret) {
        _clr_overlay(PU_RED);
        _printf("Failed to seek offset %lx from start of data.arc with errno %d\n", offset, ret);
        return ret;
    }

    return 0;
}

bool ZSTDFileIsFrame(const char* filePath) {
  const size_t magicSize = 4;
  unsigned char buf[magicSize];
  FILE* file = fopen(filePath, "rb");
  fread(buf, magicSize, sizeof(unsigned char), file);
  fclose(file);
  return ZSTD_isFrame(buf, magicSize);
}

char* compressFile(const char* path, u64 compSize, u64 &dataSize)  // returns pointer to heap
{
  char* outBuff = new char[compSize];
  FILE* inFile = fopen(path, "rb");
  fseek(inFile, 0, SEEK_END);
  u64 inSize = ftell(inFile);
  fseek(inFile, 0, SEEK_SET);
  char* inBuff = new char[inSize];
  fread(inBuff, sizeof(char), inSize, inFile);
  fclose(inFile);
  int compLvl = 0;
  if(compContext == nullptr) compContext = ZSTD_createCCtx();
  do
  {
    dataSize = ZSTD_compressCCtx(compContext, outBuff, compSize, inBuff, inSize, compLvl++);
    if(compLvl==8) compLvl = 17;  // skip arbitrary amount of levels for speed.
  }
  while (ZSTD_isError(dataSize) && compLvl <= ZSTD_maxCLevel() && strcmp(ZSTD_getErrorName(dataSize), "Destination buffer is too small") == 0);
  //printf("Compression level: %d\n", compLvl-1);
  if(ZSTD_isError(dataSize))
  {
    delete[] outBuff;
    outBuff = nullptr;
  }
  delete[] inBuff;
  return outBuff;
}
// Forward declaration for use in minBackup()
int load_mod(const char* path, uint64_t offset, FILE* arc);

void minBackup(u64 modSize, u64 offset, FILE* arc) {

    char* backup_path = new char[FILENAME_SIZE];
    snprintf(backup_path, FILENAME_SIZE, "%s0x%lx.backup", backups_root, offset);

    if (fileExists(std::string(backup_path))) {
        if(modSize > std::experimental::filesystem::file_size(backup_path)) {
            load_mod(backup_path, offset, arc);
        }
        else {
          _clr_overlay(PU_BLUE);
          _printf("Backup file 0x%lx.backup already exists\n", offset);
          delete[] backup_path;
          return;
        }
    }

    fseek(arc, offset, SEEK_SET);
    char* buf = new char[modSize];
    fread(buf, sizeof(char), modSize, arc);

    FILE* backup = fopen(backup_path, "wb");
    if (backup) fwrite(buf, sizeof(char), modSize, backup);
    else { _clr_overlay(PU_RED); _printf("Attempted to create backup file '%s', failed to get backup file handle\n", backup_path); }
    fclose(backup);
    delete[] buf;
    delete[] backup_path;
    return;
}

int load_mod(const char* path, uint64_t offset, FILE* arc) {
    std::array<u64, 3> fileData;
    u64 compSize = 0;
    u64 decompSize = 0;
    char* compBuf = nullptr;
    u64 realCompSize = 0;
    std::string pathStr(path);
    u64 modSize = std::experimental::filesystem::file_size(path);

    if(pathStr.substr(pathStr.find_last_of('/'), 3) != "/0x") {
        if(offsetObj == nullptr && std::filesystem::exists(offsetDBPath)) {
            _printf("Parsing Offsets.txt\n");
            
            offsetObj = new offsetFile(offsetDBPath);
        }
        if(offsetObj != nullptr) {
            //_printf("Looking up compression size in Offsets.txt\n");
            //
            std::string arcPath = pathStr.substr(pathStr.find('/',pathStr.find("mods/")+5)+1);
            fileData = offsetObj->getKey(arcPath);
            compSize = fileData[1];
            decompSize = fileData[2];
            if(modSize > decompSize) {
              _printf("Mod can not be larger than expected uncompressed size\n");
              return -1;
            }
            if(compSize != decompSize && !ZSTDFileIsFrame(path)) {
                if(compSize != 0) {
                    _printf("Compressing...\n");
                    
                    compBuf = compressFile(path, compSize, realCompSize);
                    if (compBuf == nullptr)
                    {
                        _clr_overlay(PU_RED);
                        _printf("Compression failed\n");
                        return -1;
                    }
                }
                // should never happen, only mods with an Offsets entry get here
                else { _clr_overlay(PU_RED); _printf("comp size not found\n"); }
            }
            else _printf("No compression needed\n");
        }
    }
    if(pathStr.find(backups_root) == std::string::npos) {
        if(compSize > 0) minBackup(compSize, offset, arc);
        else minBackup(modSize, offset, arc);
    }
    if(compBuf != nullptr) {
        u64 headerSize = ZSTD_frameHeaderSize(compBuf, compSize);
        //u64 frameSize = ZSTD_findFrameCompressedSize(compBuf, compSize);
        u64 paddingSize = (compSize - realCompSize);
        fseek(arc, offset, SEEK_SET);
        fwrite(compBuf, sizeof(char), headerSize, arc);
        char* zBuff = new char[paddingSize];
        memset(zBuff, 0, paddingSize);
        if (paddingSize % 3 != 0) {
            if (paddingSize % 3 == 1) zBuff[paddingSize-4] = 2;
            else if (paddingSize % 3 == 2) {
                zBuff[paddingSize-4] = 2;
                zBuff[paddingSize-8] = 2;
            }
        }
        fwrite(zBuff, sizeof(char), paddingSize, arc);
        delete[] zBuff;
        fwrite(compBuf+headerSize, sizeof(char), (realCompSize - headerSize), arc);
        delete[] compBuf;
    }
    else{
        FILE* f = fopen(path, "rb");
        if(f) {
            int ret = seek_files(f, offset, arc);
            if (!ret) {
                void* copy_buffer = malloc(FILE_READ_SIZE);
                uint64_t total_size = 0;

                // Copy in up to FILE_READ_SIZE byte chunks
                size_t size;
                do {
                    size = fread(copy_buffer, 1, FILE_READ_SIZE, f);
                    total_size += size;

                    fwrite(copy_buffer, 1, size, arc);
                } while(size == FILE_READ_SIZE);

                free(copy_buffer);
            }

            fclose(f);
        } else {
            _clr_overlay(PU_RED);
            _printf("Found file '%s', failed to get file handle\n", path);
            return -1;
        }
    }

    return 0;
}
/*
int create_backup(const char* mod_dir, char* filename, uint64_t offset, FILE* arc) {  // Not used
    char* backup_path = (char*) malloc(FILENAME_SIZE);
    char* mod_path = (char*) malloc(FILENAME_SIZE);
    snprintf(backup_path, FILENAME_SIZE, "%s0x%lx.backup", backups_root, offset);
    snprintf(mod_path, FILENAME_SIZE, "%s%s/%s", manager_root, mod_dir, filename);

    if (fileExists(std::string(backup_path))) {
        _printf(CONSOLE_BLUE "Backup file 0x%lx.backup already exists\n" CONSOLE_RESET, offset);
        free(backup_path);
        free(mod_path);
        return 0;
    }

    FILE* backup = fopen(backup_path, "wb");
    FILE* mod = fopen(mod_path, "rb");
    if(backup && mod) {
        fseek(mod, 0, SEEK_END);
        size_t filesize = ftell(mod);
        int ret = seek_files(backup, offset, arc);
        if (!ret) {
            void* copy_buffer = malloc(FILE_READ_SIZE);
            uint64_t total_size = 0;

            // Copy in up to FILE_READ_SIZE byte chunks
            size_t size;
            do {
                size_t to_read = FILE_READ_SIZE;
                if (filesize < FILE_READ_SIZE)
                    to_read = filesize;
                size = fread(copy_buffer, 1, to_read, arc);
                total_size += size;
                filesize -= size;

                fwrite(copy_buffer, 1, size, backup);
            } while(size == FILE_READ_SIZE);

            free(copy_buffer);
        }

        fclose(backup);
        fclose(mod);
    } else {
        if (backup) fclose(backup);
        else _printf(CONSOLE_RED "Attempted to create backup file '%s', failed to get backup file handle\n" CONSOLE_RESET, backup_path);

        if (mod) fclose(mod);
        else _printf(CONSOLE_RED "Attempted to create backup file '%s', failed to get mod file handle '%s'\n" CONSOLE_RESET, backup_path, mod_path);
    }

    free(backup_path);
    free(mod_path);

    return 0;
}
*/

#define UC(c) ((unsigned char)c)

char _isxdigit (unsigned char c)
{
    if (( c >= UC('0') && c <= UC('9') ) ||
        ( c >= UC('a') && c <= UC('f') ) ||
        ( c >= UC('A') && c <= UC('F') ))
        return 1;
    return 0;
}

unsigned char xtoc(char x) {
    if (x >= UC('0') && x <= UC('9'))
        return x - UC('0');
    else if (x >= UC('a') && x <= UC('f'))
        return (x - UC('a')) + 0xa;
    else if (x >= UC('A') && x <= UC('F'))
        return (x - UC('A')) + 0xA;
    return -1;
}

uint64_t hex_to_u64(char* str) {
    uint64_t value = 0;
    if(str[0] == '0' && str[1] == 'x') {
        str += 2;
        while(_isxdigit(*str)) {
            value *= 0x10;
            value += xtoc(*str);
            str++;
        }
    }
    return value;
}

void add_mod_dir(const char* path) {
    mod_dirs = (char**) realloc(mod_dirs, ++num_mod_dirs * sizeof(const char*));
    mod_dirs[num_mod_dirs-1] = (char*) malloc(FILENAME_SIZE * sizeof(char));
    strcpy(mod_dirs[num_mod_dirs-1], path);
}

void remove_last_mod_dir() {
    free(mod_dirs[num_mod_dirs-1]);
    num_mod_dirs--;
}

int load_mods(FILE* f_arc) {
    std::string mod_dir = mod_dirs[num_mod_dirs-1];

    remove_last_mod_dir();

    DIR *d;
    struct dirent *dir;

    _clr_overlay(PU_YELLOW);
    _printf("Searching mod dir %s\n\n", mod_dir.c_str());
    _main->CallForRender();

    std::string abs_mod_dir = std::string(manager_root) + mod_dir;
    d = opendir(abs_mod_dir.c_str());
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if(dir->d_type == DT_DIR) {
                if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                    continue;
                std::string new_mod_dir = mod_dir + "/" + dir->d_name;
                add_mod_dir(new_mod_dir.c_str());
            } else {
                uint64_t offset = hex_to_u64(dir->d_name);
                if(!offset) {
                    if(offsetObj == nullptr && std::filesystem::exists(offsetDBPath)) {
                        _printf("Parsing Offsets.txt\n");
                        _main->CallForRender();
                        offsetObj = new offsetFile(offsetDBPath);
                    }
                    if(offsetObj != nullptr) {
                        //_printf("Trying to find offset in Offsets.txt\n");
                        //
                        std::string arcFileName = (mod_dir.substr(mod_dir.find('/', mod_dir.find('/')+1) + 1) + "/" + dir->d_name);
                        offset = offsetObj->getOffset(arcFileName);
                    }
                }
                if(offset){
                    if (mod_dir == "backups") {
                        std::string backup_file = std::string(backups_root) + std::string(dir->d_name);
                        load_mod(backup_file.c_str(), offset, f_arc);

                        remove(backup_file.c_str());
                        _clr_overlay(PU_BLUE);
                        _printf("%s\n\n", dir->d_name);
                        _main->CallForRender();
                    } else {
                        std::string mod_file = std::string(manager_root) + mod_dir + "/" + dir->d_name;
                        if (installing == INSTALL) {
                            appletSetCpuBoostMode(ApmCpuBoostMode_Type1);
                            load_mod(mod_file.c_str(), offset, f_arc);
                            appletSetCpuBoostMode(ApmCpuBoostMode_Disabled);
                            _clr_overlay(PU_GREEN);
                            _printf("%s/%s\n\n", mod_dir.c_str(), dir->d_name);
                            _main->CallForRender();
                        } else if (installing == UNINSTALL) {
                            char* backup_path = (char*) malloc(FILENAME_SIZE);
                            snprintf(backup_path, FILENAME_SIZE, "%s0x%lx.backup", backups_root, offset);

                            if(std::filesystem::exists(backup_path)) {
                                load_mod(backup_path, offset, f_arc);
                                remove(backup_path);
                                _clr_overlay(PU_BLUE);
                                _printf("%s\n\n", mod_file.c_str());
                            }
                            else { _clr_overlay(PU_RED); _printf("No backup found\n\n"); }
                            free(backup_path);
                            _main->CallForRender();
                        }
                    }
                } else {
                    _clr_overlay(PU_RED);
                    _printf("Found file '%s', offset not parsable\n", dir->d_name);
                    _main->CallForRender();
                }
            }
        }
        closedir(d);
    } else {
        _clr_overlay(PU_RED);
        _printf("Failed to open mod directory '%s'\n", abs_mod_dir.c_str());
        _main->CallForRender();
    }

    return 0;
}

void perform_installation() {
    std::string arc_path = "sdmc:/" + getCFW() + "/titles/01006A800016E000/romfs/data.arc";
    FILE* f_arc;
    if(!std::filesystem::exists(arc_path)) {
      _clr_overlay(PU_RED);
      _printf("\nNo data.arc found!\n");
      _clr_overlay(PU_GREEN);
      _printf("Please use the Data Arc Dumper first.\n");
      goto end;
    }
    f_arc = fopen(arc_path.c_str(), "r+b");
    if(!f_arc){
        _clr_overlay(PU_RED);
        _printf("Failed to get file handle to data.arc\n");
        goto end;
    }
    if (installing == INSTALL)
        _printf("\nInstalling mods...\n\n");
    else if (installing == UNINSTALL)
        _printf("\nUninstalling mods...\n\n");
    
    while (num_mod_dirs > 0) {
        _main->CallForRender();
        load_mods(f_arc);
    }

    free(mod_dirs);

    fclose(f_arc);

end:
    _printf("Mod Installer finished.\nPress B to return to the Mod Installer.\n");
    _printf("Press X to launch Smash\n\n");
    installation_finish = true;
}