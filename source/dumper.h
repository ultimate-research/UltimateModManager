#include <string.h>
#include <stdio.h>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <ctime>

#include <switch.h>
#include "utils.h"
#include <mbedtls/md5.h>

bool dump_done = false;
bool exfat = false;
bool verifyDump = false;
std::string outPath = dataArcPath(getCFW());
const int MD5_DIGEST_LENGTH = 16;

void md5HashFromFile(std::string filename, unsigned char* out)
{
    FILE *inFile = fopen (filename.c_str(), "rb");
    mbedtls_md5_context md5Context;
    int bytes;
    u64 bufSize = 512000;
    unsigned char data[bufSize];

    if (inFile == NULL)
    {
      printf (CONSOLE_RED "\nThe data.arc file can not be opened." CONSOLE_RESET);
      return;
    }
    mbedtls_md5_init (&md5Context);
    mbedtls_md5_starts_ret(&md5Context);

    fseek(inFile, 0, SEEK_END);
    u64 size = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);
    u64 sizeRead = 0;
    int percent = 0;
    while ((bytes = fread (data, 1, bufSize, inFile)) != 0)
    {
      mbedtls_md5_update_ret (&md5Context, data, bytes);
      sizeRead += bytes;
      percent = sizeRead * 100 / size;
      print_progress(percent, 100);
      consoleUpdate(NULL);
    }
    mbedtls_md5_finish_ret (&md5Context, out);
    fclose(inFile);
    return;
}


void copy(const char* from, const char* to, bool exfat = false)
{
    Result rc=0;
    //const u64 fat32Max = 0xFFFFFFFF;
    //const u64 splitSize = 0xFFFF0000;
    const u64 smashTID = 0x01006A800016E000;
    u64 bufSize = 0x0F116C00;

    if(runningTID() != smashTID)
    {
      printf(CONSOLE_RED "\nYou must override Smash for this application to work properly.\nHold 'R' while launching Smash to do so." CONSOLE_RESET);
      return;
    }
    if (!applicationMode)
    {
      printf(CONSOLE_RED "\nNo applet mode.\nYou must override Smash for this application to work properly.\nHold 'R' while launching Smash to do so." CONSOLE_RESET);
      return;
    }
    std::string backups = "/UltimateModManager/backups";
    removeRecursive(backups);
    mkdir(backups.c_str(), 0777);
    remove(tablePath);
    rc = fsFsDeleteFile(fsdevGetDeviceFileSystem("sdmc"), outPath.c_str());
    if(R_FAILED(rc))
    {
        // 0x202 = Path does not exist.
        if(rc == 0x202)
        {
            rc = fsdevDeleteDirectoryRecursively(outPath.c_str());
            if(R_FAILED(rc) && rc != 0x202)
            {
                printf(CONSOLE_RED "\nFailed to remove the data.arc folder. error: 0x%x" CONSOLE_RESET, rc);
                return;
            }
        }
        else
        {
            printf(CONSOLE_RED "\nFailed to remove the data.arc. error: 0x%x" CONSOLE_RESET, rc);
            return;
        }
    }
    romfsMountFromCurrentProcess("romfs");
    FILE* source = fopen(from, "rb");
    if(source == nullptr)
    {
      printf (CONSOLE_RED "\nThe romfs could not be read." CONSOLE_RESET);
      fclose(source);
      romfsUnmount("romfs");
	    return;
    }
    fseek(source, 0, SEEK_END);
    u64 size = ftell(source);
    fseek(source, 0, SEEK_SET);

    std::uintmax_t spaceAvailable = std::filesystem::space(to).available;
    if(spaceAvailable < size)
    {
      double mbAvailable = spaceAvailable/1024.0/1024.0;
      double mbNeeded = size/1024.0/1024.0;
      printf(CONSOLE_RED "\nNot enough storage space on the SD card.\nYou need %.2f MB, you have %.2f MB available" CONSOLE_RESET, mbNeeded, mbAvailable);
      fclose(source);
      romfsUnmount("romfs");
      return;
    }
    std::string folder(to);
    folder = folder.substr(0, folder.find_last_of("/"));
    if(!std::filesystem::exists(folder))
    {
      mkdirs(folder, 0744);
    }
    if(!exfat) {
      rc = fsdevCreateFile(to, 0, FsCreateOption_BigFile);
      if (R_FAILED(rc)) {
        printf("\nfsdevCreateFile() failed: 0x%x", rc);
        fclose(source);
        romfsUnmount("romfs");
        return;
      }
    }

    FILE* dest = fopen(to, "wb+");
    if(dest == nullptr)
    {
      printf(CONSOLE_RED "\nCould not open the destination file. error: %s" CONSOLE_RESET, strerror(errno));
      fclose(dest);
      fclose(source);
      romfsUnmount("romfs");
      return;
    }

    char* buf = new char[bufSize];
    u64 sizeWritten = 0;
    size_t ret;
    int percent = 0;
    u32 srcCRC;
    u32 destCRC;
    if(size == 0)
      printf(CONSOLE_RED "\nThere was a problem opening the data.arc" CONSOLE_RESET);
    while(sizeWritten < size)
    {
      if(sizeWritten + bufSize > size)
      {
        delete[] buf;
        bufSize = size-sizeWritten;
        buf = new char[bufSize];
      }
      fread(buf, sizeof(char), bufSize, source);
      ret = fwrite(buf, sizeof(char), bufSize, dest);
      if(ret != bufSize)
      {
        printf(CONSOLE_RED "\nSomething went wrong!" CONSOLE_RESET);
        if(sizeWritten > 0 && exfat)
          printf(CONSOLE_YELLOW "\nYou sure your sd card is exfat?" CONSOLE_RESET);
        fclose(dest);
        fclose(source);
        romfsUnmount("romfs");
        return;
      }
      if(verifyDump)
      {
          srcCRC = crc32Calculate(buf, bufSize);
          fseek(dest, -bufSize, SEEK_CUR);
          fread(buf, sizeof(char), bufSize, dest);
          destCRC = crc32Calculate(buf, bufSize);
          if(srcCRC != destCRC)
          {
              printf(CONSOLE_RED "\nVerification failed. An error has occured in writing the file. Halting dump." CONSOLE_RESET);
              fclose(dest);
              fclose(source);
              romfsUnmount("romfs");
              fsFsDeleteFile(fsdevGetDeviceFileSystem("sdmc"), outPath.c_str());
              return;
          }
      }
      sizeWritten += bufSize;
      percent = sizeWritten * 100 / size;
      print_progress(percent, 100);
      //printf("\x1b[20;2Hdest pos: %lld, source pos: %lld", (long long int)dest.tellp(), (long long int)source.tellg());  // Debug log
      //printf("\x1b[22;2H%lu/%lu", sizeWritten, size);  // Debug log
      consoleUpdate(NULL);
    }
    fclose(source);
    fclose(dest);
    delete[] buf;
    romfsUnmount("romfs");
    //printf("\n");
}

void dumperMainLoop(int kDown) {
    u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
    if (kDown & KEY_X)
    {
        if(kHeld & KEY_R) {
            if(std::filesystem::exists(outPath))
            {
              printf("\nBeginning hash generation...\n" CONSOLE_ESC(s));
              consoleUpdate(NULL);
              unsigned char out[MD5_DIGEST_LENGTH];
              u64 startTime = std::time(0);
              // Should I block home button here too?
              appletSetMediaPlaybackState(true);
              md5HashFromFile(outPath, out);
              appletSetMediaPlaybackState(false);
              u64 endTime = std::time(0);
              printf("\nMD5:");
              for(int i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", out[i]);
              printf("\nHashing took %.2f minutes", (float)(endTime - startTime)/60);
              consoleUpdate(NULL);
              shortVibratePattern();
            }
            else
            {
              printf(CONSOLE_RED "\nNo data.arc file found on the SD card." CONSOLE_RESET);
            }
        }
        else {
            appletRequestLaunchApplication(smashTID, NULL);
        }
    }
    if (kDown & KEY_Y) exfat = true;
    if (kDown & KEY_A) exfat = false;
    if ((kDown & KEY_A || kDown & KEY_Y) && !dump_done)
    {
        if(kHeld & KEY_R)
        {
            verifyDump = true;
            printf("\nBeginning the dumping process with verification...\n" CONSOLE_ESC(s));
        }
        else
        {
            verifyDump = false;
            printf("\nBeginning the dumping process...\n" CONSOLE_ESC(s));
        }
        consoleUpdate(NULL);
        u64 startTime = std::time(0);
        appletBeginBlockingHomeButton(0);
        appletSetMediaPlaybackState(true);
        copy("romfs:/data.arc", outPath.c_str(), exfat);
        appletEndBlockingHomeButton();
        appletSetMediaPlaybackState(false);
        u64 endTime = std::time(0);

        dump_done = true;  // So you don't accidentally dump twice
        printf("\nCompleted in %.2f minutes.", (float)(endTime - startTime)/60);
        printf("\nPress 'X' to verify the dump by launching smash");
        printf("\nPress 'B' to return to the main menu\n");
        consoleUpdate(NULL);
        shortVibratePattern();
    }

    if (kDown & KEY_B) {
        menu = mainMenu;
        printMainMenu();
    }
}
