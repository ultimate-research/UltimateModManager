#include <switch.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

#define FILENAME_SIZE 0x120
#define FILE_READ_SIZE 0x5000

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
            printf("Installed file '%s' with 0x%lx bytes\n", path, total_size);
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
    snprintf(backup_path, FILENAME_SIZE, "sdmc:/SaltySD/backups/0x%lx.backup", offset);
    snprintf(mod_path, FILENAME_SIZE, "sdmc:/SaltySD/%s/%s", mod_dir, filename);

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
            printf("Created backup '%s' with 0x%lx bytes\n", backup_path, total_size);
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

    char* tmp = (char*) (FILENAME_SIZE);
    DIR *d;
    struct dirent *dir;
    int hours, minutes, seconds;

    printf("Searching mod dir '%s'...\n", mod_dir);
    
    snprintf(tmp, FILENAME_SIZE, "sdmc:/SaltySD/%s/", mod_dir);

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
                    printf("Found file '%s', offset = %ld\n", dir->d_name, offset);
                    if (strcmp(mod_dir, "backups") == 0) {
                        current_time(&hours, &minutes, &seconds);
                        printf("[%02d:%02d:%02d] About to install backup '%s'\n", hours, minutes, seconds, tmp);
                        snprintf(tmp, FILENAME_SIZE, "sdmc:/SaltySD/backups/%s", dir->d_name);
                        load_mod(tmp, offset, f_arc);

                        remove(tmp);
                        current_time(&hours, &minutes, &seconds);
                        printf("[%02d:%02d:%02d] Installed backup '%s' into arc and deleted it\n", hours, minutes, seconds, tmp);
                    } else {
                        current_time(&hours, &minutes, &seconds);
                        printf("[%02d:%02d:%02d] About to create backup '%s'\n", hours, minutes, seconds, dir->d_name);
                        create_backup(mod_dir, dir->d_name, offset, f_arc);

                        snprintf(tmp, FILENAME_SIZE, "sdmc:/SaltySD/%s/%s", mod_dir, dir->d_name);
                        printf("[%02d:%02d:%02d] About to install mod '%s/%s'\n", hours, minutes, seconds, mod_dir, dir->d_name);
                        load_mod(tmp, offset, f_arc);
                        current_time(&hours, &minutes, &seconds);
                        printf("[%02d:%02d:%02d] Installed mod '%s' into arc\n", hours, minutes, seconds, tmp);
                    }
                } else {
                    printf("Found file '%s', offset not parsable\n", dir->d_name);
                }
            }
        }
        closedir(d);
    } else {
        printf("Failed to open mod directory '%s'\n", mod_dir);
    }

    free(tmp);
    free(mod_dir);
    return 0;
}

void modInstallerMainLoop(int kDown)
{
    printf("BEGIN: SALTYSD MOD INSTALLER\n\n");

    FILE* f_arc = fopen("sdmc:/atmosphere/titles/01006A800016E000/romfs/data.arc", "r+b");
    if(!f_arc){
        printf("Failed to get file handle to data.arc\n");
        goto end;
    }

    // restore backups -> delete backups -> make backups for current mods -> install current mods
    printf("Installing backups...\n\n");
    add_mod_dir("backups");
    load_mods(f_arc);
    
    printf("Installing mods...\n\n");
    add_mod_dir("mods");
    while (num_mod_dirs > 0) {
        load_mods(f_arc);
    }

    free(mod_dirs);

    fclose(f_arc);

end:
    printf("END: SALTYSD MOD INSTALLER\n\n");
}
