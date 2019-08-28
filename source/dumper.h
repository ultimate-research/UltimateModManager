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
std::string outPath = "sdmc:/" + getCFW() + "/titles/01006A800016E000/romfs/data.arc";
const int MD5_DIGEST_LENGTH = 16;

void md5HashFromFile(std::string filename, unsigned char* out)
{
    FILE *inFile = fopen (filename.c_str(), "rb");
    mbedtls_md5_context md5Context;
    int bytes;
    u64 bufSize = 500000;
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
      printf("\x1b[s\n%d/100\x1b[u", percent);
      consoleUpdate(NULL);
    }
    mbedtls_md5_finish_ret (&md5Context, out);
    fclose(inFile);
    return;
}


void copy(const char* from, const char* to, bool exfat = false)
{
    //const u64 fat32Max = 0xFFFFFFFF;
    //const u64 splitSize = 0xFFFF0000;
    const u64 smashTID = 0x01006A800016E000;
    u64 bufSize = 0x0F0F0F0F;

    if(runningTID() != smashTID)
    {
      printf(CONSOLE_RED "\nYou must override Smash for this application to work properly.\nHold 'R' while launching Smash to do so." CONSOLE_RESET);
      return;
    }
    AppletType at = appletGetAppletType();
    if (at != AppletType_Application && at != AppletType_SystemApplication)
    {
      printf(CONSOLE_RED "\nNo applet mode.\nYou must override Smash for this application to work properly.\nHold 'R' while launching Smash to do so." CONSOLE_RESET);
      return;
    }
    remove(outPath.c_str());
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

    if(std::filesystem::space(to).available < size)
    {
      printf(CONSOLE_RED "\nNot enough storage space on the SD card." CONSOLE_RESET);
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
    if(!exfat)
      fsdevCreateFile(to, 0, FS_CREATE_BIG_FILE);

    FILE* dest = fopen(to, "wb");
    if(dest == nullptr)
    {
      printf(CONSOLE_RED "\nCould not open the destination file." CONSOLE_RESET);
      fclose(dest);
      fclose(source);
      romfsUnmount("romfs");
      return;
    }

    char* buf = new char[bufSize];
    u64 sizeWritten = 0;
    int percent = 0;
    size_t ret;

    if(size == 0)
      printf(CONSOLE_RED "\nThere might be a problem with the data.arc file on your SD card. Please remove the file manually." CONSOLE_RESET);
    while(sizeWritten < size)
    {
      if(sizeWritten + bufSize > size)
      {
        delete[] buf;
        bufSize = size-sizeWritten;
        buf = new char[bufSize];
      }


      /* Broken automatic fat32 detection
      if(!FSChecked && sizeWritten > fat32Max)
      {
        // Write one over the limit
        // delete[] buf;
        // u64 tempBufSize = (fat32Max - sizeWritten) + 1;
        // buf = new char[tempBufSize];
        // source.read(buf, tempBufSize);
        // dest.write(buf, tempBufSize);
        // delete[] buf;
        // buf = new char[bufSize];

        if(dest.bad())  // assuming this is caused by fat32 size limit
        {
          dest.close();
          printf("\x1b[30;2HFat32 detected, making split file");
          std::filesystem::resize_file(to, splitSize);
          std::rename(to, (std::string(to) + "temp").c_str());
          mkdir(to, 0744);
          std::rename((std::string(to) + "temp").c_str(), (std::string(to) + "/00").c_str());

          //fsdevCreateFile((std::string(to) + "/01").c_str(), 0, 0);

          fsdevSetArchiveBit(to);

          // re-write chopped off peice next time
          source.seekg(splitSize);
          if(source.fail())
            printf("\nsource failed after seeking");

          // hopefully that was the only problem with the stream
          dest.clear();
          dest.open(to);
          dest.seekp(splitSize);
          //printf("\nsplit Pos:%lld\nsplitSize:%ld", (long long int)dest.tellp(), splitSize);
          if(dest.fail())
            printf("\ncould not reopen destination file");

          sizeWritten = splitSize;

          source.seekg(splitSize);
          printf("\nafter split: dest pos: %lld, source pos: %lld", (long long int)dest.tellp(), (long long int)source.tellg());
        }
        FSChecked = true;
      }
      */
      fread(buf, sizeof(char), bufSize, source);
      ret = fwrite(buf, sizeof(char), bufSize, dest);
      if(ret != bufSize)
      {
        printf(CONSOLE_RED "\nSomething went wrong!" CONSOLE_RESET);
        fclose(dest);
        fclose(source);
        romfsUnmount("romfs");
        return;
      }
      sizeWritten += bufSize;
      percent = sizeWritten * 100 / size;
      printf("\x1b[s\n%d/100\x1b[u", percent);
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
    if (kDown & KEY_X)
    {
        if(std::filesystem::exists(outPath))
        {
          printf("\nBeginning hash generation...");
          consoleUpdate(NULL);
          unsigned char out[MD5_DIGEST_LENGTH];
          u64 startTime = std::time(0);
          // Should I block home button here too?
          appletSetMediaPlaybackState(true);
          md5HashFromFile(outPath, out);
          appletSetMediaPlaybackState(false);
          u64 endTime = std::time(0);
          printf("\nmd5:");
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
    if (kDown & KEY_Y && !dump_done) exfat = true;
    if ((kDown & KEY_A || kDown & KEY_Y) && !dump_done)
    {
        printf("\nBeginning the dumping process...");
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
        printf("\nOptional: Press 'X' generate an MD5 hash of the file");
        printf("\nPress B to return to the main menu.\n");
        consoleUpdate(NULL);
        shortVibratePattern();
    }

    if (kDown & KEY_B) {
        menu = MAIN_MENU;
        printMainMenu();
    }
}
