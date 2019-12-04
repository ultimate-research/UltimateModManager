#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <list>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include "utils.h"
#include "arcReader.h"
#include <stdarg.h>
#include <ctime>

#define FILE_READ_SIZE 0x20000

ArcReader* arcReader = nullptr;

bool uninstall = false;
bool deleteMod = false;
bool pendingTableWrite = false;
typedef struct ModFile {
    std::string mod_path;
    s64 offset;

    ModFile(std::string mod_path, s64 offset)
        : mod_path(std::move(mod_path)), offset(offset)
    {}
} ModFile;
std::vector<ModFile> mod_files;

bool installation_finish = false;
s64 mod_folder_index = 0;
int smashVersion;
ZSTD_CCtx* compContext = nullptr;
std::list<s64> installIDXs;
std::list<std::string> InstalledMods;
std::list<std::string> modDirList;

const char* mods_root = "sdmc:/UltimateModManager/mods/";
const char* backups_root = "sdmc:/UltimateModManager/backups/";
std::string arc_path = "sdmc:/" + getCFW() + "/titles/01006A800016E000/romfs/data.arc";

int regionIndex = getRegion();

void updateInstalledList() {
    InstalledMods.clear();
    for(auto& dirEntry: std::filesystem::directory_iterator(backups_root)) {
        if(dirEntry.is_directory())
            InstalledMods.push_back(dirEntry.path().filename());
    }
}

bool ZSTDFileIsFrame(const char* filePath) {
  const size_t magicSize = 4;
  unsigned char buf[magicSize];
  FILE* file = fopen(filePath, "rb");
  fread(buf, magicSize, sizeof(unsigned char), file);
  fclose(file);
  return ZSTD_isFrame(buf, magicSize);
}

bool paddable(u64 padSize) {
    return !(padSize == 1 || padSize == 2 || padSize == 5);
}

void setZSTDCustomLvl(ZSTD_CCtx* cctx, int level) {
    if(level <= ZSTD_maxCLevel())
        ZSTD_CCtx_setParameter(compContext, ZSTD_c_compressionLevel, level);
    else if(level == ZSTD_maxCLevel() + 1) {
        ZSTD_CCtx_setParameter(compContext, ZSTD_c_compressionLevel, 22);
        ZSTD_CCtx_setParameter(compContext, ZSTD_c_strategy, ZSTD_btopt);
    }
    else if(level == ZSTD_maxCLevel() + 2) {
        ZSTD_CCtx_setParameter(compContext, ZSTD_c_compressionLevel, 22);
        ZSTD_CCtx_setParameter(compContext, ZSTD_c_minMatch, 4);
        ZSTD_CCtx_setParameter(compContext, ZSTD_c_strategy, ZSTD_greedy);
    }
}

char* compressFile(const char* path, u64 compSize, u64 &dataSize)  // returns pointer to heap
{
  const int ZSTDCustom_maxCLevel = ZSTD_maxCLevel() + 2;
  u64 bufSize = compSize+0x100;
  char* outBuff = new char[bufSize];
  FILE* inFile = fopen(path, "rb");
  fseek(inFile, 0, SEEK_END);
  u64 inSize = ftell(inFile);
  fseek(inFile, 0, SEEK_SET);
  char* inBuff = new char[inSize];
  fread(inBuff, sizeof(char), inSize, inFile);
  fclose(inFile);
  int compLvl = 3;
  u64 bytesAway = ULONG_MAX;
  if(compContext == nullptr) compContext = ZSTD_createCCtx();
   // Minimize header size
  ZSTD_CCtx_setParameter(compContext, ZSTD_c_contentSizeFlag, 1);
  ZSTD_CCtx_setParameter(compContext, ZSTD_c_checksumFlag, 0);
  ZSTD_CCtx_setParameter(compContext, ZSTD_c_dictIDFlag, 1);
  do {
    setZSTDCustomLvl(compContext, compLvl++);
    dataSize = ZSTD_compress2(compContext, outBuff, bufSize, inBuff, inSize);
    if(!ZSTD_isError(dataSize) && dataSize > compSize) {
        debug_log("Compressed \"%s\" to %lu bytes, %lu bytes away from goal, at level %d.\n", path, dataSize, dataSize - compSize, compLvl-1);
        if(dataSize - compSize < bytesAway)
            bytesAway = dataSize - compSize;
    }
    if(compLvl==10) compLvl = 17;  // skip arbitrary amount of levels for speed.
  }
  while((dataSize > compSize || ZSTD_isError(dataSize) || !paddable(compSize - dataSize)) && compLvl <= ZSTDCustom_maxCLevel);

  if(compLvl > ZSTDCustom_maxCLevel) {
      if(bytesAway != ULONG_MAX)
          log("%lu bytes too large ", bytesAway);
      delete[] outBuff;
      outBuff = nullptr;
  }

  delete[] inBuff;
  return outBuff;
}
// Forward declaration for use in backup()
int load_mod(const char* path, long offset, FILE* arc);

void backup(char* modName, u64 modSize, u64 offset, FILE* arc) {
    int fileNameSize = snprintf(nullptr, 0, "%s%s/0x%lx.backup", backups_root, modName, offset) + 1;
    char* backup_path = new char[fileNameSize];
    snprintf(backup_path, fileNameSize, "%s%s/0x%lx.backup", backups_root, modName, offset);
    std::string parentPath = std::filesystem::path(backup_path).parent_path();
    if(!std::filesystem::exists(parentPath))
        mkdir(parentPath.c_str(), 0777);
    {
        bool duplicateBackup = false;
        std::filesystem::path dupeDirEntry;
        for(auto& dirEntry: std::filesystem::recursive_directory_iterator(backups_root)) {
            if(dirEntry.is_regular_file() && dirEntry.path().filename() == std::filesystem::path(backup_path).filename()) {
                dupeDirEntry = dirEntry.path();
                duplicateBackup = true;
                break;
            }
        }
        if(duplicateBackup) {
            if(modSize > std::filesystem::file_size(dupeDirEntry)) {
                load_mod(dupeDirEntry.c_str(), offset, arc);
            }
            else {
                debug_log("Backup file 0x%lx.backup already exists\n", offset);
                std::filesystem::rename(dupeDirEntry, backup_path);
                rmdir(dupeDirEntry.parent_path().c_str());
                delete[] backup_path;
                return;
            }
        }
    }

    fseek(arc, offset, SEEK_SET);
    char* buf = new char[modSize];
    fread(buf, sizeof(char), modSize, arc);

    FILE* backup = fopen(backup_path, "wb");
    if(backup)
        fwrite(buf, sizeof(char), modSize, backup);
    else
        log(CONSOLE_RED "Attempted to create backup file '%s', failed to get file handle\n" CONSOLE_RESET, backup_path);
    fclose(backup);
    delete[] buf;
    delete[] backup_path;
    return;
}

int load_mod(const char* path, long offset, FILE* arc) {
    u32 compSize = 0;
    u32 decompSize = 0;
    char* compBuf = nullptr;
    u64 realCompSize = 0;
    std::string pathStr(path);
    u64 modSize = std::filesystem::file_size(path);

    if(pathStr.substr(pathStr.find_last_of('/'), 3) != "/0x") {
        if(arcReader == nullptr) {
            arcReader = new ArcReader(arc_path.c_str());
            if(!arcReader->isInitialized()) {
                arcReader = nullptr;
            }
        }
        if(arcReader != nullptr) {
            if(arcReader->Version != smashVersion) {
                log(CONSOLE_RED "Your data.arc does not match your game version\n" CONSOLE_RESET);
                return -1;
            }
            std::string arcFileName = pathStr.substr(pathStr.find('/',pathStr.find("mods/")+5)+1);
            bool regional;
            arcReader->GetFileInformation(arcFileName, offset, compSize, decompSize, regional, regionIndex);
            bool infoUpdated = false;
            if(compSize == decompSize && modSize > decompSize)
                if(arcReader->updateFileInfo(arcFileName, 0, 0, decompSize+1, SUBFILE_COMPRESSED_ZSTD) != -1) {
                    infoUpdated = true;
                    decompSize++;
                }
            if(modSize > decompSize) {
                if(arcReader->updateFileInfo(arcFileName, 0, 0, modSize) != -1)
                    infoUpdated = true;
                else {
                    log(CONSOLE_RED "%s can not be larger than expected uncompressed size\n" CONSOLE_RESET, path);
                    return -1;
                }
            }
            if(compSize != decompSize && !ZSTDFileIsFrame(path)) {
                compBuf = compressFile(path, compSize, realCompSize);
                if(compBuf == nullptr)
                {
                    log(CONSOLE_RED "Compression failed on %s\n" CONSOLE_RESET, path);
                    return -1;
                }
            }
            if(infoUpdated)
                pendingTableWrite = true;
        }
    }
    if(pathStr.find(backups_root) == std::string::npos) {
        const char* modNameStart = path+strlen(mods_root);
        u32 modNameSize = (u32)(strchr(modNameStart, '/') - modNameStart);
        char* modName = new char[modNameSize+1];
        strncpy(modName, modNameStart, modNameSize);
        modName[modNameSize] = 0;
        if(compSize > 0)
            backup(modName, compSize, offset, arc);
        else
            backup(modName, modSize, offset, arc);
        delete[] modName;
    }
    if(compBuf != nullptr) {
        u64 headerSize = ZSTD_frameHeaderSize(compBuf, compSize);
        u64 paddingSize = compSize - realCompSize;
        fseek(arc, offset, SEEK_SET);
        fwrite(compBuf, sizeof(char), headerSize, arc);
        char* zBuff = new char[paddingSize];
        memset(zBuff, 0, paddingSize);
        if(paddingSize % 3 != 0) {
            if(paddingSize >= 4 && paddingSize % 3 == 1) zBuff[paddingSize-4] = 2;
            else if(paddingSize >= 8 && paddingSize % 3 == 2) {
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
            int ret = fseek(f, 0, SEEK_SET);
            if(ret != 0) {
                log(CONSOLE_RED "Failed to seek file with errno %d\n" CONSOLE_RESET, ret);
                fclose(f);
                return -1;
            }
            ret = fseek(arc, offset, SEEK_SET);
            if(ret != 0) {
                log(CONSOLE_RED "Failed to seek offset %lx from start of data.arc with errno %d\n" CONSOLE_RESET, offset, ret);
                fclose(f);
                return -1;
            }
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
            fclose(f);
        }
        else {
            log(CONSOLE_RED "Found file '%s', failed to get file handle\n" CONSOLE_RESET, path);
            return -1;
        }
    }
    return 0;
}

#define UC(c) ((unsigned char)c)

char _isxdigit (unsigned char c) {
    if(( c >= UC('0') && c <= UC('9') ) ||
        ( c >= UC('a') && c <= UC('f') ) ||
        ( c >= UC('A') && c <= UC('F') ))
        return 1;
    return 0;
}

unsigned char xtoc(char x) {
    if(x >= UC('0') && x <= UC('9'))
        return x - UC('0');
    else if(x >= UC('a') && x <= UC('f'))
        return (x - UC('a')) + 0xa;
    else if(x >= UC('A') && x <= UC('F'))
        return (x - UC('A')) + 0xA;
    return -1;
}

uint64_t hex_to_u64(const char* str) {
    uint64_t value = 0;
    int idx = 0;
    if(str[0] == '0' && str[1] == 'x') {
        idx += 2;
        while(_isxdigit(str[idx])) {
            value *= 0x10;
            value += xtoc(str[idx]);
            idx++;
        }
    }
    return value;
}

void load_mods(FILE* f_arc) {
    size_t num_mod_files = mod_files.size();
    printf(CONSOLE_ESC(s));
    for(size_t i = 0; i < num_mod_files; i++) {
        print_progress(i, num_mod_files);
        consoleUpdate(NULL);
        std::string mod_path = mod_files[i].mod_path;
        std::string rel_mod_dir;
        bool backupsFolder = mod_path.find(backups_root) != std::string::npos;
        if(backupsFolder)
            rel_mod_dir = mod_path.substr(strlen(backups_root));
        else
            rel_mod_dir = mod_path.substr(strlen(mods_root));
        std::string arcFileName = rel_mod_dir.substr(rel_mod_dir.find('/')+1);
        s64 offset = mod_files[i].offset;
        if(offset == 0) {
            if(arcReader == nullptr) {
                fclose(f_arc);
                arcReader = new ArcReader(arc_path.c_str());
                if(!arcReader->isInitialized())
                    arcReader = nullptr;
                f_arc = fopen(arc_path.c_str(), "r+b");
            }
            if(arcReader != nullptr) {
                u32 compSize, decompSize;
                bool regional;
                arcReader->GetFileInformation(arcFileName, offset, compSize, decompSize, regional);
            }
        }

        if(offset == 0) {
            log(CONSOLE_RED "\"%s\" was not found in the data.arc and no offset was in its name\n" CONSOLE_RESET "   Make sure the file name and path are correct.\n", arcFileName.c_str());
            continue;
        }
        const char* mod_path_c_str = mod_path.c_str();
        if(backupsFolder) {
            load_mod(mod_path_c_str, offset, f_arc);
            remove(mod_path_c_str);
            std::string parentPath = std::filesystem::path(mod_path).parent_path();
            rmdir(parentPath.c_str());
        }
        else {
            if(!uninstall) {
                appletSetCpuBoostMode(ApmCpuBoostMode_Type1);
                load_mod(mod_path_c_str, offset, f_arc);
                appletSetCpuBoostMode(ApmCpuBoostMode_Disabled);
                debug_log("installed \"%s\"\n", mod_path_c_str);
            }
            else {
                const char* modNameStart = mod_path_c_str+strlen(mods_root);
                u32 modNameSize = (u32)(strchr(modNameStart, '/') - modNameStart);
                char* modName = new char[modNameSize+1];
                strncpy(modName, modNameStart, modNameSize);
                modName[modNameSize] = 0;
                int fileNameSize = snprintf(nullptr, 0, "%s%s/0x%lx.backup", backups_root, modName, offset) + 1;
                char* backup_path = new char[fileNameSize];
                snprintf(backup_path, fileNameSize, "%s%s/0x%lx.backup", backups_root, modName, offset);
                if(std::filesystem::exists(backup_path)) {
                    load_mod(backup_path, offset, f_arc);
                    remove(backup_path);
                    debug_log("restored \"%s\"\n", backup_path);
                }
                else
                    log(CONSOLE_RED "backup '0x%lx' does not exist\n" CONSOLE_RESET, offset);
                std::string parentPath = std::filesystem::path(backup_path).parent_path();
                free(backup_path);
                rmdir(parentPath.c_str());
            }
        }
    }
    print_progress(num_mod_files, num_mod_files);
    if(num_mod_files == 0)
        printf("Note: " CONSOLE_YELLOW "Nothing installed\n" CONSOLE_RESET);
    consoleUpdate(NULL);
    mod_files.clear();
}

int enumerate_mod_files(FILE* f_arc) {
    std::string mod_dir = modDirList.back();
    modDirList.pop_back();
    std::error_code ec;
    s64 offset;
    std::filesystem::path modpath;
    auto fsit = std::filesystem::recursive_directory_iterator(std::filesystem::path(mod_dir), ec);
    if(!ec) {
        for(auto& dirEntry: fsit) {
            if(dirEntry.is_regular_file()) {
                modpath = dirEntry.path();
                offset = hex_to_u64(modpath.filename().c_str());
                mod_files.emplace_back(modpath, offset);
            }
        }
    }
    else {
        log(CONSOLE_RED "Failed to open mod directory '%s'\n" CONSOLE_RESET, mod_dir.c_str());
    }
    return 0;
}

void perform_installation() {
    std::string rootModDir = modDirList.back();
    FILE* f_arc;
    if(!std::filesystem::exists(arc_path) || std::filesystem::is_directory(std::filesystem::status(arc_path))) {
      log(CONSOLE_RED "\nNo data.arc found!\n" CONSOLE_RESET
      "   Please use the " CONSOLE_GREEN "Data Arc Dumper" CONSOLE_RESET " first.\n");
      goto end;
    }
    f_arc = fopen(arc_path.c_str(), "r+b");
    if(!f_arc) {
        log(CONSOLE_RED "Failed to get file handle to data.arc\n" CONSOLE_RESET);
        goto end;
    }
    if(!uninstall)
        printf("\nInstalling mods...\n\n");
    else
        printf("\nUninstalling mods...\n\n");
    consoleUpdate(NULL);
    while(!modDirList.empty()) {
        enumerate_mod_files(f_arc);
    }
    load_mods(f_arc);
    if(pendingTableWrite) {
        printf("Writing file table\n");
        consoleUpdate(NULL);
        arcReader->writeFileInfo(f_arc);
        pendingTableWrite = false;
    }
    fclose(f_arc);
    if(deleteMod) {
      printf("Deleting mod files\n");
      fsdevDeleteDirectoryRecursively(rootModDir.c_str());
    }

end:
    if(!errorLogs.empty()) {
        printf("Error Logs:\n");
        for(std::string line : errorLogs) {
            printf(line.c_str());
        }
        errorLogs.clear();
    }
    else
        printf(CONSOLE_GREEN "Successful\n" CONSOLE_RESET);
    printf("Press B to return to the Mod Installer.\n");
    printf("Press X to launch Smash\n\n");
}

void modInstallerMainLoop(int kDown)
{
    u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
    if(!installation_finish) {
        consoleClear();
        if(kHeld & KEY_RSTICK_DOWN) {
            svcSleepThread(7e+7);
            mod_folder_index++;
        }
        if(kHeld & KEY_RSTICK_UP) {
            svcSleepThread(7e+7);
            mod_folder_index--;
        }
        if(kDown & KEY_ZR) {
            std::list<s64>::iterator it = std::find(installIDXs.begin(), installIDXs.end(), mod_folder_index);
            if(it == installIDXs.end())
                installIDXs.push_back(mod_folder_index);
            else
                installIDXs.erase(it);
        }
        if(kDown & KEY_DDOWN || kDown & KEY_LSTICK_DOWN)
            mod_folder_index++;
        else if(kDown & KEY_DUP || kDown & KEY_LSTICK_UP)
            mod_folder_index--;

        bool start_install = false;
        deleteMod = false;
        if(kDown & KEY_Y) {
            if(kHeld & KEY_L && kHeld & KEY_R)
                deleteMod = true;
            start_install = true;
            uninstall = true;
        }
        else if(kDown & KEY_A) {
            start_install = true;
            uninstall = false;
        }
        printf("\n");
        std::error_code ec;
        auto fsit = std::filesystem::directory_iterator(std::filesystem::path(mods_root), ec);
        if(!ec) {
            s64 curr_folder_index = 0;
            std::filesystem::path path;
            for(auto& dirEntry: fsit) {
                if(dirEntry.is_directory()) {
                    path = dirEntry.path();
                    if(std::find(InstalledMods.begin(), InstalledMods.end(), path.filename()) != InstalledMods.end())
                        printf(CONSOLE_BLUE);
                    if(curr_folder_index == mod_folder_index) {
                        printf("%s> ", CONSOLE_GREEN);
                        if(start_install) {
                            modDirList.emplace_back(dirEntry.path());
                        }
                    }
                    else if(std::find(installIDXs.begin(), installIDXs.end(), curr_folder_index) != installIDXs.end()) {
                        printf(CONSOLE_CYAN);
                        if(start_install) {
                            modDirList.emplace_back(dirEntry.path());
                        }
                    }
                    if(curr_folder_index < 42 || curr_folder_index <= mod_folder_index)
                        printf("%s\n", path.filename().c_str());
                    printf(CONSOLE_RESET);
                    curr_folder_index++;
                }
            }

            if(mod_folder_index < 0)
                mod_folder_index = curr_folder_index;

            if(mod_folder_index > curr_folder_index)
                mod_folder_index = 0;

            if(mod_folder_index == curr_folder_index) {
                printf(CONSOLE_GREEN "> ");
                if(start_install) {
                    modDirList.push_back(backups_root);
                    if(arcReader == nullptr) {
                        arcReader = new ArcReader(arc_path.c_str());
                        if(!arcReader->isInitialized()) {
                            arcReader = nullptr;
                        }
                    }
                    arcReader->restoreTable();
                }
            }

            if(curr_folder_index < 42 || curr_folder_index <= mod_folder_index)
                printf(CONSOLE_YELLOW "Restore backups and file table\n" CONSOLE_RESET);

            if(mod_folder_index == curr_folder_index)
                printf(CONSOLE_RESET);

            if(applicationMode)
                console_set_status(GREEN "\nMod Installer " VERSION_STRING " " RESET);
            else
                console_set_status(GREEN "\nMod Installer " CONSOLE_RED "                                       APPLET MODE NOT RECOMMENDED" RESET);
            printf(CONSOLE_ESC(s) CONSOLE_ESC(46;1H) GREEN "A" RESET "=Install "
                   GREEN "Y" RESET "=Uninstall " GREEN "L+R+Y" RESET "=Delete "
                   GREEN "R-Stick" RESET "=Scroll " GREEN "ZR" RESET "=Multi-select "
                   GREEN "B" RESET "=Main Menu" CONSOLE_ESC(u));
        }
        else {
            log(CONSOLE_RED "%s folder not found\n\n" CONSOLE_RESET, mods_root);
        }

        consoleUpdate(NULL);
        if(start_install) {
            consoleClear();
            perform_installation();
            installation_finish = true;
            modDirList.clear();
            installIDXs.clear();
            updateInstalledList();
        }
    }

    if(kDown & KEY_B) {
        if(installation_finish) {
            installation_finish = false;
            consoleClear();
        }
        else {
          if(arcReader != nullptr) {
              delete arcReader;
              arcReader = nullptr;
          }
          if(compContext != nullptr) {
              ZSTD_freeCCtx(compContext);
              compContext = nullptr;
          }
          menu = MAIN_MENU;
          printMainMenu();
        }
    }
    if(kDown & KEY_X) {
        appletRequestLaunchApplication(smashTID, NULL);
    }
    if(kHeld & KEY_L && kHeld & KEY_R && kDown & KEY_MINUS) {
        if(arcReader == nullptr) {
            arcReader = new ArcReader(arc_path.c_str());
            if(!arcReader->isInitialized()) {
                arcReader = nullptr;
            }
        }
        arcReader->restoreTable();
    }
}
