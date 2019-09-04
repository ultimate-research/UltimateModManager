#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <experimental/filesystem>
#include "utils.h"
#include "offsetFile.h"

#define FILENAME_SIZE 0x130
#define FILE_READ_SIZE 0x20000

char** mod_dirs = NULL;
size_t num_mod_dirs = 0;
bool installation_finish = false;
s64 mod_folder_index = 0;
offsetFile* offsetObj = nullptr;

const char* manager_root = "sdmc:/UltimateModManager/";
const char* mods_root = "sdmc:/UltimateModManager/mods/";
const char* backups_root = "sdmc:/UltimateModManager/backups/";
const char* offsetDBPath = "sdmc:/UltimateModManager/Offsets.txt";

int seek_files(FILE* f, uint64_t offset, FILE* arc) {
    // Set file pointers to start of file and offset respectively
    int ret = fseek(f, 0, SEEK_SET);
    if (ret) {
        printf(CONSOLE_RED "Failed to seek file with errno %d\n" CONSOLE_RESET, ret);
        return ret;
    }

    ret = fseek(arc, offset, SEEK_SET);
    if (ret) {
        printf(CONSOLE_RED "Failed to seek offset %lx from start of data.arc with errno %d\n" CONSOLE_RESET, offset, ret);
        return ret;
    }

    return 0;
}

void minBackup(u64 modSize, u64 offset, FILE* arc) {

    char* backup_path = new char[FILENAME_SIZE];
    snprintf(backup_path, FILENAME_SIZE, "%s0x%lx.backup", backups_root, offset);

    if (fileExists(std::string(backup_path)) && modSize <= std::experimental::filesystem::file_size(backup_path)) {
        printf(CONSOLE_BLUE "Backup file 0x%lx.backup already exists\n" CONSOLE_RESET, offset);
        delete[] backup_path;
        return;
    }

    fseek(arc, offset, SEEK_SET);
    char* buf = new char[modSize];
    fread(buf, sizeof(char), modSize, arc);

    FILE* backup = fopen(backup_path, "wb");
    if (backup) fwrite(buf, sizeof(char), modSize, backup);
    else printf(CONSOLE_RED "Attempted to create backup file '%s', failed to get backup file handle\n" CONSOLE_RESET, backup_path);
    fclose(backup);
    delete[] buf;
    delete[] backup_path;
    return;
}

int load_mod(const char* path, uint64_t offset, FILE* arc) {
    u64 compSize = 0;
    char* compBuf = nullptr;
    u64 realCompSize = 0;
    std::string pathStr(path);
    u64 modSize = std::experimental::filesystem::file_size(path);

    if(pathStr.substr(pathStr.find_last_of('/'), 3) != "/0x") {
        if(offsetObj == nullptr && std::filesystem::exists(offsetDBPath)) {
            printf("Parsing Offsets.txt\n");
            consoleUpdate(NULL);
            offsetObj = new offsetFile(offsetDBPath);
        }
        if(offsetObj != nullptr) {
            printf("Looking up compression size in Offsets.txt\n");
            consoleUpdate(NULL);
            std::string arcPath = pathStr.substr(pathStr.find('/',pathStr.find("mods/")+5)+1);
            compSize = offsetObj->getCompSize(arcPath);
            if(compSize != modSize) {
                if(compSize != 0) {
                    printf("Compressing...\n");
                    consoleUpdate(NULL);
                    compBuf = compressFile(path, compSize, realCompSize);
                    if (compBuf == nullptr)
                    {
                        printf(CONSOLE_RED "Compression failed\n" CONSOLE_RESET);
                        return -1;
                    }
                }
                // should never happen, only mods with an Offsets entry get here
                else printf(CONSOLE_RED "comp size not found\n" CONSOLE_RESET);
            }
            else printf("No compression needed\n");
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
            printf(CONSOLE_RED "Found file '%s', failed to get file handle\n" CONSOLE_RESET, path);
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
        printf(CONSOLE_BLUE "Backup file 0x%lx.backup already exists\n" CONSOLE_RESET, offset);
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
        else printf(CONSOLE_RED "Attempted to create backup file '%s', failed to get backup file handle\n" CONSOLE_RESET, backup_path);

        if (mod) fclose(mod);
        else printf(CONSOLE_RED "Attempted to create backup file '%s', failed to get mod file handle '%s'\n" CONSOLE_RESET, backup_path, mod_path);
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

    printf("Searching mod dir " CONSOLE_YELLOW "%s\n\n" CONSOLE_RESET, mod_dir.c_str());
    consoleUpdate(NULL);

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
                        printf("Parsing Offsets.txt\n");
                        consoleUpdate(NULL);
                        offsetObj = new offsetFile(offsetDBPath);
                    }
                    if(offsetObj != nullptr) {
                        printf("Trying to find offset in Offsets.txt\n");
                        consoleUpdate(NULL);
                        std::string arcFileName = (mod_dir.substr(mod_dir.find('/', mod_dir.find('/')+1) + 1) + "/" + dir->d_name);
                        offset = offsetObj->getOffset(arcFileName);
                    }
                }
                if(offset){
                    if (mod_dir == "backups") {
                        std::string backup_file = std::string(backups_root) + std::string(dir->d_name);
                        load_mod(backup_file.c_str(), offset, f_arc);

                        remove(backup_file.c_str());
                        printf(CONSOLE_BLUE "%s\n\n" CONSOLE_RESET, dir->d_name);
                        consoleUpdate(NULL);
                    } else {
                        //create_backup(mod_dir.c_str(), dir->d_name, offset, f_arc);  // Needs to be done elsewhere

                        std::string mod_file = std::string(manager_root) + mod_dir + "/" + dir->d_name;
                        appletSetCpuBoostMode(ApmCpuBoostMode_Type1);
                        load_mod(mod_file.c_str(), offset, f_arc);
                        appletSetCpuBoostMode(ApmCpuBoostMode_Disabled);
                        printf(CONSOLE_GREEN "%s/%s\n\n" CONSOLE_RESET, mod_dir.c_str(), dir->d_name);
                        consoleUpdate(NULL);
                    }
                } else {
                    printf(CONSOLE_RED "Found file '%s', offset not parsable\n" CONSOLE_RESET, dir->d_name);
                    consoleUpdate(NULL);
                }
            }
        }
        closedir(d);
    } else {
        printf(CONSOLE_RED "Failed to open mod directory '%s'\n" CONSOLE_RESET, abs_mod_dir.c_str());
        consoleUpdate(NULL);
    }

    return 0;
}

void perform_installation() {
    std::string arc_path = "sdmc:/" + getCFW() + "/titles/01006A800016E000/romfs/data.arc";
    FILE* f_arc;
    if(!std::filesystem::exists(arc_path)) {
      printf(CONSOLE_RED "\nNo data.arc found!\n" CONSOLE_RESET);
      printf("Please use the " CONSOLE_GREEN "Data Arc Dumper" CONSOLE_RESET " first.\n");
      goto end;
    }
    f_arc = fopen(arc_path.c_str(), "r+b");
    if(!f_arc){
        printf(CONSOLE_RED "Failed to get file handle to data.arc\n" CONSOLE_RESET);
        goto end;
    }

    printf("\nInstalling mods...\n\n");
    consoleUpdate(NULL);
    while (num_mod_dirs > 0) {
        consoleUpdate(NULL);
        load_mods(f_arc);
    }

    free(mod_dirs);

    fclose(f_arc);

end:
    printf("Mod Installer finished.\nPress B to return to the Mod Installer.\n");
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
        if (kDown & KEY_DDOWN || kDown & KEY_LSTICK_DOWN)
            mod_folder_index++;
        else if (kDown & KEY_DUP || kDown & KEY_LSTICK_UP)
            mod_folder_index--;

        bool start_install = false;
        if (kDown & KEY_A)
            start_install = true;
        bool found_dir = false;

        printf("Please select a mods folder below to install.\n\n");

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
                    printf("%s\n", dir->d_name);
                    if (curr_folder_index == mod_folder_index)
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

            printf("backups\n");

            if (mod_folder_index == curr_folder_index)
                printf(CONSOLE_RESET);

            closedir(d);
        } else {
            printf(CONSOLE_RED "%s folder not found\n\n" CONSOLE_RESET, mods_root);
        }

        consoleUpdate(NULL);
        if (start_install && found_dir) {
            mkdir(backups_root, 0777);
            perform_installation();
            installation_finish = true;
            mod_dirs = NULL;
            num_mod_dirs = 0;
        }
    }

    if (kDown & KEY_B) {
        if (installation_finish) {
          installation_finish = false;
          consoleClear();
        }
        else {
          if(offsetObj != nullptr) {
              delete offsetObj;
              offsetObj = nullptr;
          }
          menu = MAIN_MENU;
          printMainMenu();
        }
    }
    if(kDown & KEY_X) {
        appletRequestLaunchApplication(0x01006A800016E000, NULL);
    }
}
