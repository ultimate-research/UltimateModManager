#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <list>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <experimental/filesystem>
#include "utils.h"
#include "ArcReader.h"
#include "crc32.h"
#include <stdarg.h>

//#define IS_DEBUG

#ifdef IS_DEBUG
#include <ctime>
#endif

#define FILENAME_SIZE 0x130
#define FILE_READ_SIZE 0x20000

#define INSTALL false
#define UNINSTALL true

ArcReader* arcReader = nullptr;

bool installing = INSTALL;
bool deleteMod = false;

char** mod_dirs = NULL;
size_t num_mod_dirs = 0;
typedef struct ModFile {
    std::string mod_path;
    long offset;
} ModFile;
std::vector<ModFile> mod_files;

bool installation_finish = false;
s64 mod_folder_index = 0;
ZSTD_CCtx* compContext = nullptr;
std::list<s64> installIDXs;
std::vector<std::string> errorLogs;

const char* manager_root = "sdmc:/UltimateModManager/";
const char* mods_root = "sdmc:/UltimateModManager/mods/";
const char* backups_root = "sdmc:/UltimateModManager/backups/";
std::string arc_path = "sdmc:/" + getCFW() + "/titles/01006A800016E000/romfs/data.arc";

enum smashRegions{
    jp_ja,
    us_en,
    us_fr,
    us_es,
    eu_en,
    eu_fr,
    eu_es,
    eu_de,
    eu_nl,
    eu_it,
    eu_ru,
    kr_ko,
    zh_cn,
    zh_tw
};

const std::map<std::string, int> regionMap {
    {"ja",      jp_ja},
    {"en-US",   us_en},
    {"fr",      eu_fr},
    {"de",      eu_de},
    {"it",      eu_it},
    {"es",      eu_es},
    {"zh-CN",   zh_cn},
    {"ko",      kr_ko},
    {"nl",      eu_nl},
    {"pt",      eu_en},
    {"ru",      eu_ru},
    {"zh-TW",   zh_tw},
    {"en-GB",   eu_en},
    {"fr-CA",   us_fr},
    {"es-419",  us_es},
    {"zh-Hans", zh_cn},
    {"zh-Hant", zh_tw},
};

void log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args) + 1;
    char* buffer = new char[len];
    vsnprintf(buffer, len, fmt, args);

    std::string logLine = std::string(buffer);
    delete[] buffer;
    errorLogs.push_back(logLine);
}

#ifdef IS_DEBUG
 #define debug_log(...) \
     {char buf[10]; \
     std::time_t now = std::time(0); \
     std::strftime(buf, sizeof(buf), "%T", std::localtime(&now)); \
     printf("[%s] ", buf); \
     printf(__VA_ARGS__); \
     consoleUpdate(NULL);}
#else
#define debug_log(...)
#endif

int getRegion() {
    u64 languageCode;
    //setGetLanguageCode(&languageCode);
    appletGetDesiredLanguage(&languageCode);
    return regionMap.find((char*)&languageCode)->second;
}

int regionIndex = getRegion();

int seek_files(FILE* f, uint64_t offset, FILE* arc) {
    // Set file pointers to start of file and offset respectively
    int ret = fseek(f, 0, SEEK_SET);
    if (ret) {
        log(CONSOLE_RED "Failed to seek file with errno %d\n" CONSOLE_RESET, ret);
        return ret;
    }

    ret = fseek(arc, offset, SEEK_SET);
    if (ret) {
        log(CONSOLE_RED "Failed to seek offset %lx from start of data.arc with errno %d\n" CONSOLE_RESET, offset, ret);
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
  char* outBuff = new char[compSize+1];
  FILE* inFile = fopen(path, "rb");
  fseek(inFile, 0, SEEK_END);
  u64 inSize = ftell(inFile);
  fseek(inFile, 0, SEEK_SET);
  char* inBuff = new char[inSize];
  fread(inBuff, sizeof(char), inSize, inFile);
  fclose(inFile);
  int compLvl = 3;
  if(compContext == nullptr) compContext = ZSTD_createCCtx();
  ZSTD_parameters params;
  params.fParams = {0,0,1};  // Minimize header size
  do
  {
    params.cParams = ZSTD_getCParams(compLvl++, inSize, 0);
    dataSize = ZSTD_compress_advanced(compContext, outBuff, compSize+1, inBuff, inSize, nullptr, 0, params);
    if(compLvl==8) compLvl = 17;  // skip arbitrary amount of levels for speed.
  }
  while ((dataSize > compSize || ZSTD_isError(dataSize)) && compLvl <= ZSTD_maxCLevel());

  if(dataSize > compSize || ZSTD_isError(dataSize))
  {
    delete[] outBuff;
    outBuff = nullptr;
  }

  delete[] inBuff;
  return outBuff;
}
// Forward declaration for use in minBackup()
int load_mod(const char* path, long offset, FILE* arc);

void minBackup(u64 modSize, u64 offset, FILE* arc) {

    char* backup_path = new char[FILENAME_SIZE];
    snprintf(backup_path, FILENAME_SIZE, "%s0x%lx.backup", backups_root, offset);

    if (fileExists(std::string(backup_path))) {
        if(modSize > std::experimental::filesystem::file_size(backup_path)) {
            load_mod(backup_path, offset, arc);
        }
        else {
          debug_log(CONSOLE_BLUE "Backup file 0x%lx.backup already exists\n" CONSOLE_RESET, offset);
          delete[] backup_path;
          return;
        }
    }

    fseek(arc, offset, SEEK_SET);
    char* buf = new char[modSize];
    fread(buf, sizeof(char), modSize, arc);

    FILE* backup = fopen(backup_path, "wb");
    if (backup) fwrite(buf, sizeof(char), modSize, backup);
    else log(CONSOLE_RED "Attempted to create backup file '%s', failed to get file handle\n" CONSOLE_RESET, backup_path);
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
    u64 modSize = std::experimental::filesystem::file_size(path);

    if(pathStr.substr(pathStr.find_last_of('/'), 3) != "/0x") {
        if(arcReader == nullptr) {
            debug_log("Parsing data.arc\n");
            consoleUpdate(NULL);
            arcReader = new ArcReader(arc_path.c_str());
            debug_log("Done parsing\n");
            consoleUpdate(NULL);
        }
        if(arcReader != nullptr) {
            std::string arcFileName = pathStr.substr(pathStr.find('/',pathStr.find("mods/")+5)+1);
            bool regional;
            arcReader->GetFileInformation(arcFileName, offset, compSize, decompSize, regional, regionIndex);
            if(modSize > decompSize) {
              log(CONSOLE_RED "%s can not be larger than expected uncompressed size\n" CONSOLE_RESET, path);
              return -1;
            }
            if(compSize != decompSize && !ZSTDFileIsFrame(path)) {
                if(compSize != 0) {
                    compBuf = compressFile(path, compSize, realCompSize);
                    if (compBuf == nullptr)
                    {
                        log(CONSOLE_RED "Compression failed on %s\n" CONSOLE_RESET, path);
                        return -1;
                    }
                }
                // should never happen, only mods with an Offsets entry get here
                else log(CONSOLE_RED "comp size not found for %s\n" CONSOLE_RESET, path);
            }
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
            log(CONSOLE_RED "Found file '%s', failed to get file handle\n" CONSOLE_RESET, path);
            return -1;
        }
    }

    return 0;
}

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

void load_mods(FILE* f_arc) {
    size_t num_mod_files = mod_files.size();
    size_t i = 0;

    printf(CONSOLE_ESC(s));

    for (ModFile mod_file : mod_files) {
        std::string mod_path = mod_file.mod_path;
        std::string rel_mod_dir;
        if (mod_path.find("backups") != std::string::npos)
            rel_mod_dir = mod_path.substr(strlen(backups_root));
        else
            rel_mod_dir = mod_path.substr(strlen(mods_root));
        std::string arcFileName = rel_mod_dir.substr(rel_mod_dir.find('/')+1);
        print_progress(i, num_mod_files);
        consoleUpdate(NULL);
        long offset = mod_file.offset;
        if(!offset) {
            if(arcReader == nullptr) {
                debug_log("Parsing data.arc\n");
                consoleUpdate(NULL);
                fclose(f_arc);
                arcReader = new ArcReader(arc_path.c_str());
                f_arc = fopen(arc_path.c_str(), "r+b");
                debug_log("Done parsing\n");
                consoleUpdate(NULL);
            }
            if(arcReader != nullptr) {
                u32 compSize, decompSize;
                bool regional;
                arcReader->GetFileInformation(arcFileName, offset, compSize, decompSize, regional);
            }
        }

        if (!offset) {
            log(CONSOLE_RED "Found file '%s', offset not found.\n" CONSOLE_RESET "   Make sure the file name and/or path is correct.\n", arcFileName.c_str());
            i++;
            continue;
        }

        if (mod_path.find("backups") != std::string::npos) {
            load_mod(mod_path.c_str(), offset, f_arc);

            remove(mod_path.c_str());
            debug_log(CONSOLE_BLUE "%s\n\n" CONSOLE_RESET, mod_path.c_str());
        } else {
            if (installing == INSTALL) {
                appletSetCpuBoostMode(ApmCpuBoostMode_Type1);
                load_mod(mod_path.c_str(), offset, f_arc);
                appletSetCpuBoostMode(ApmCpuBoostMode_Disabled);
                debug_log(CONSOLE_GREEN "%s\n\n" CONSOLE_RESET, mod_path.c_str());
            } else if (installing == UNINSTALL) {
                char* backup_path = (char*) malloc(FILENAME_SIZE);
                snprintf(backup_path, FILENAME_SIZE, "%s0x%lx.backup", backups_root, offset);

                if(std::filesystem::exists(backup_path)) {
                    load_mod(backup_path, offset, f_arc);
                    remove(backup_path);
                    debug_log(CONSOLE_BLUE "%s\n\n" CONSOLE_RESET, backup_path);
                }
                else log(CONSOLE_RED "backup '0x%x' does not exist\n" CONSOLE_RESET, offset);
                free(backup_path);
            }
        }

        i++;
    }

    print_progress(i, num_mod_files);
    consoleUpdate(NULL);

    mod_files.clear();
}

int enumerate_mod_files(FILE* f_arc) {
    std::string mod_dir = mod_dirs[num_mod_dirs-1];

    remove_last_mod_dir();

    DIR *d;
    struct dirent *dir;

    debug_log("Searching mod dir " CONSOLE_YELLOW "%s\n\n" CONSOLE_RESET, mod_dir.c_str());

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
                long offset = hex_to_u64(dir->d_name);
                if (mod_dir == "backups") {
                    std::string backup_file = std::string(backups_root) + std::string(dir->d_name);
                    mod_files.push_back(ModFile{backup_file, offset});
                } else {
                    std::string mod_file = std::string(manager_root) + mod_dir + "/" + dir->d_name;
                    mod_files.push_back(ModFile{mod_file, offset});
                }
            }
        }
        closedir(d);
    } else {
        log(CONSOLE_RED "Failed to open mod directory '%s'\n" CONSOLE_RESET, abs_mod_dir.c_str());
    }

    return 0;
}

void perform_installation() {
    std::string rootModDir = std::string(manager_root) + mod_dirs[num_mod_dirs-1];

    FILE* f_arc;
    if(!std::filesystem::exists(arc_path) || std::filesystem::is_directory(std::filesystem::status(arc_path))) {
      log(CONSOLE_RED "\nNo data.arc found!\n" CONSOLE_RESET
      "   Please use the " CONSOLE_GREEN "Data Arc Dumper" CONSOLE_RESET " first.\n");
      goto end;
    }
    f_arc = fopen(arc_path.c_str(), "r+b");
    if(!f_arc){
        log(CONSOLE_RED "Failed to get file handle to data.arc\n" CONSOLE_RESET);
        goto end;
    }
    if (installing == INSTALL)
        printf("\nInstalling mods...\n\n");
    else if (installing == UNINSTALL)
        printf("\nUninstalling mods...\n\n");
    consoleUpdate(NULL);
    while (num_mod_dirs > 0) {
        enumerate_mod_files(f_arc);
    }

    free(mod_dirs);

    load_mods(f_arc);

    fclose(f_arc);
    if (deleteMod) {
      printf("Deleting mod files\n");
      fsdevDeleteDirectoryRecursively(rootModDir.c_str());
    }

end:
    if (!errorLogs.empty()) {
        printf("Error Logs:\n");
        for (std::string line : errorLogs) {
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
    if (!installation_finish) {
        consoleClear();
        u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
        if (kHeld & KEY_RSTICK_DOWN) {
            svcSleepThread(7e+7);
            mod_folder_index++;
        }
        if (kHeld & KEY_RSTICK_UP) {
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
        if (kDown & KEY_DDOWN || kDown & KEY_LSTICK_DOWN)
            mod_folder_index++;
        else if (kDown & KEY_DUP || kDown & KEY_LSTICK_UP)
            mod_folder_index--;

        bool start_install = false;
        deleteMod = false;
        if(kHeld & KEY_L && kHeld & KEY_R && kDown & KEY_Y) {
          deleteMod = true;
          start_install = true;
          installing = UNINSTALL;
        }
        else if (kDown & KEY_A) {
            start_install = true;
            installing = INSTALL;
        }
        else if (kDown & KEY_Y) {
            start_install = true;
            installing = UNINSTALL;
        }
        bool found_dir = false;
        printf("\n");

        DIR* d = opendir(mods_root);
        struct dirent *dir;
        if (d) {
            s64 curr_folder_index = 0;
            while ((dir = readdir(d)) != NULL) {
                std::string d_name = std::string(dir->d_name);
                if(dir->d_type == DT_DIR) {
                    if (d_name == "." || d_name == "..")
                        continue;
                    std::string directory = "mods/" + d_name;

                    if (curr_folder_index == mod_folder_index) {
                        printf("%s> ", CONSOLE_GREEN);
                        if (start_install) {
                            found_dir = true;
                            add_mod_dir(directory.c_str());
                        }
                    }
                    else if(std::find(installIDXs.begin(), installIDXs.end(), curr_folder_index) != installIDXs.end()) {
                        printf(CONSOLE_CYAN);
                        if (start_install) {
                            add_mod_dir(directory.c_str());
                        }
                    }
                    if(curr_folder_index < 42 || curr_folder_index <= mod_folder_index)
                        printf("%s\n", dir->d_name);
                    printf(CONSOLE_RESET);
                    curr_folder_index++;
                }
            }

            if (mod_folder_index < 0)
                mod_folder_index = curr_folder_index;

            if (mod_folder_index > curr_folder_index)
                mod_folder_index = 0;

            if (mod_folder_index == curr_folder_index) {
                printf(CONSOLE_GREEN "> ");
                if (start_install) {
                    found_dir = true;
                    add_mod_dir("backups");
                }
            }

            if(curr_folder_index < 42 || curr_folder_index <= mod_folder_index)
                printf("backups\n");

            if (mod_folder_index == curr_folder_index)
                printf(CONSOLE_RESET);

            closedir(d);
            console_set_status("\n" GREEN "Mod Installer" RESET);
            printf(CONSOLE_ESC(s) CONSOLE_ESC(46;1H) GREEN "A" RESET "=install "
                   GREEN "Y" RESET "=uninstall " GREEN "L+R+Y" RESET "=delete "
                   GREEN "R-Stick" RESET "=scroll " GREEN "ZR" RESET "=multi-select "
                   GREEN "B" RESET "=main menu" CONSOLE_ESC(u));
        } else {
            log(CONSOLE_RED "%s folder not found\n\n" CONSOLE_RESET, mods_root);
        }

        consoleUpdate(NULL);
        if (start_install && found_dir) {
            consoleClear();
            mkdir(backups_root, 0777);
            perform_installation();
            installation_finish = true;
            mod_dirs = NULL;
            num_mod_dirs = 0;
            installIDXs.clear();
        }
    }

    if (kDown & KEY_B) {
        if (installation_finish) {
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
        appletRequestLaunchApplication(0x01006A800016E000, NULL);
    }
}
