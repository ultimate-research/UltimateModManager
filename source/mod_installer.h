#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

#define FILENAME_SIZE 0x130
#define FILE_READ_SIZE 0x20000

void current_time(int* hours, int* minutes, int* seconds) {
    time_t now;
    time(&now);

    struct tm *local = localtime(&now);
    *hours = local->tm_hour;
    *minutes = local->tm_min;
    *seconds = local->tm_sec;
}

int seek_files(FILE* f, uint64_t offset, FILE* arc) {
    // Set file pointers to start of file and offset respectively
    int ret = fseek(f, 0, SEEK_SET);
    if (ret) {
        printf("Failed to seek file with errno %d\n", ret);
        return ret;
    }

    ret = fseek(arc, offset, SEEK_SET);
    if (ret) {
        printf("Failed to seek offset %lx from start of data.arc with errno %d\n", offset, ret);
        return ret;
    }

    return 0;
}

int load_mod(char* path, uint64_t offset, FILE* arc) {
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
        printf("Found file '%s', failed to get file handle\n", path);
        return -1;
    }

    return 0;
}

int create_backup(char* mod_dir, char* filename, uint64_t offset, FILE* arc) {
    char* backup_path = (char*) malloc(FILENAME_SIZE);
    char* mod_path = (char*) malloc(FILENAME_SIZE);
    snprintf(backup_path, FILENAME_SIZE, "sdmc:/UltimateModManager/backups/0x%lx.backup", offset);
    snprintf(mod_path, FILENAME_SIZE, "sdmc:/UltimateModManager/%s/%s", mod_dir, filename);

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
        else printf("Attempted to create backup file '%s', failed to get backup file handle\n", backup_path);

        if (mod) fclose(mod);
        else printf("Attempted to create backup file '%s', failed to get mod file handle '%s'\n", backup_path, mod_path);
    }

    free(backup_path);
    free(mod_path);

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
    if(str[0] == '0' && str[1] == 'x')
        str += 2;
    uint64_t value = 0;
    while(_isxdigit(*str)) {
        value *= 0x10;
        value += xtoc(*str);
        str++;
    }
    return value;
}

char** mod_dirs = NULL;
size_t num_mod_dirs = 0;
bool installation_finish = false;
size_t mod_folder_index = 0;

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
    char* mod_dir = (char*) malloc(FILENAME_SIZE);
    strcpy(mod_dir, mod_dirs[num_mod_dirs-1]);

    remove_last_mod_dir();

    char* tmp = (char*) malloc(FILENAME_SIZE);
    DIR *d;
    struct dirent *dir;
    int hours, minutes, seconds;

    printf("Searching mod dir %s%s%s\n\n", CONSOLE_YELLOW, mod_dir, CONSOLE_RESET);
    consoleUpdate(NULL);
    
    snprintf(tmp, FILENAME_SIZE, "sdmc:/UltimateModManager/%s/", mod_dir);

    d = opendir(tmp);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if(dir->d_type == DT_DIR) {
                if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                    continue;
                snprintf(tmp, FILENAME_SIZE, "%s/%s", mod_dir, dir->d_name);
                add_mod_dir(tmp);
            } else {
                uint64_t offset = hex_to_u64(dir->d_name);
                if(offset){
                    if (strcmp(mod_dir, "backups") == 0) {
                        current_time(&hours, &minutes, &seconds);
                        snprintf(tmp, FILENAME_SIZE, "sdmc:/UltimateModManager/backups/%s", dir->d_name);
                        load_mod(tmp, offset, f_arc);

                        remove(tmp);
                        current_time(&hours, &minutes, &seconds);
                        printf("%s%s%s\n\n", CONSOLE_BLUE, dir->d_name, CONSOLE_RESET);
                        consoleUpdate(NULL);
                    } else {
                        current_time(&hours, &minutes, &seconds);
                        consoleUpdate(NULL);
                        create_backup(mod_dir, dir->d_name, offset, f_arc);

                        snprintf(tmp, FILENAME_SIZE, "sdmc:/UltimateModManager/%s/%s", mod_dir, dir->d_name);
                        load_mod(tmp, offset, f_arc);
                        current_time(&hours, &minutes, &seconds);
                        printf("%s%s/%s%s\n\n", CONSOLE_GREEN, mod_dir, dir->d_name, CONSOLE_RESET);
                        consoleUpdate(NULL);
                    }
                } else {
                    printf("%sFound file '%s', offset not parsable%s\n", CONSOLE_RED, dir->d_name, CONSOLE_RESET);
                    consoleUpdate(NULL);
                }
            }
        }
        closedir(d);
    } else {
        printf("Failed to open mod directory '%s'\n", mod_dir);
        consoleUpdate(NULL);
    }

    free(tmp);
    free(mod_dir);
    return 0;
}

void perform_installation() {
    FILE* f_arc = fopen("sdmc:/atmosphere/titles/01006A800016E000/romfs/data.arc", "r+b");
    if(!f_arc){
        printf("Failed to get file handle to data.arc\n");
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
    printf("Mod Installer finished.\nPress B to return to the main menu.\n");
		printf("Press X launch Smash\n\n");
}

void modInstallerMainLoop(int kDown)
{
    if (!installation_finish) {
        consoleClear();
        if (kDown & KEY_DDOWN || kDown & KEY_LSTICK_DOWN)
            mod_folder_index++;
        else if (kDown & KEY_DUP || kDown & KEY_LSTICK_UP)
            mod_folder_index--;

        if (mod_folder_index < 0)
            mod_folder_index = 0;

        bool start_install = false;
        if (kDown & KEY_A)
            start_install = true;
        bool found_dir = false;

        printf("Please select a mods folder below to install.\n\n");
        
        const char* mods_root = "sdmc:/UltimateModManager/mods";
        DIR* d = opendir(mods_root);
        struct dirent *dir;
        if (d) {
            size_t curr_folder_index = 0;
            bool select_backups = false;
            while ((dir = readdir(d)) != NULL || !select_backups) {
                if (dir == NULL) {
                    select_backups = true;
                    if (mod_folder_index > curr_folder_index)
                        mod_folder_index = curr_folder_index;

                    if (mod_folder_index == curr_folder_index) {
                        mod_folder_index = curr_folder_index;
                        printf("%s> ", CONSOLE_GREEN);
                        if (start_install) {
                            found_dir = true;
                            add_mod_dir("backups");
                        }
                    }

                    printf("backups\n");

                    if (mod_folder_index == curr_folder_index)
                        printf(CONSOLE_RESET);
                } else if(dir->d_type == DT_DIR) {
                    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                        continue;
                    char* tmp = (char*) malloc(FILENAME_SIZE);
                    snprintf(tmp, FILENAME_SIZE, "mods/%s", dir->d_name);

                    if (curr_folder_index == mod_folder_index) {
                        printf("%s> ", CONSOLE_GREEN);
                        if (start_install) {
                            found_dir = true;
                            add_mod_dir(tmp);
                        }
                    }
                    printf("%s\n", dir->d_name);
                    if (curr_folder_index == mod_folder_index)
                        printf(CONSOLE_RESET);
                    free(tmp);
                    curr_folder_index++;
                }
            }

            closedir(d);
        }

        consoleUpdate(NULL);
        if (start_install && found_dir) {
            perform_installation();
            installation_finish = true;
            mod_dirs = NULL;
            num_mod_dirs = 0;
        }
    }

    if (kDown & KEY_B) {
        menu = MAIN_MENU;
        printMainMenu();
    }
    if(kDown & KEY_X) {
        appletRequestLaunchApplication(0x01006A800016E000, NULL);
    }
}
