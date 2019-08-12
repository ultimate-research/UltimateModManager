#include <string.h>
#include <stdio.h>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <ctime>

#include <hl_md5wrapper.h>
#include <switch.h>

bool isServiceRunning(const char *serviceName) {
  Handle handle;
  bool running = R_FAILED(smRegisterService(&handle, serviceName, false, 1));

  svcCloseHandle(handle);

  if (!running)
    smUnregisterService(serviceName);

  return running;
}

std::string getCFW()
{
  if (isServiceRunning("tx"))
    return "sxos";
  else if (isServiceRunning("rnx"))
    return "reinx";
  return "atmosphere";
}

u64 runningTID()
{
  u64 pid = 0;
  u64 tid = 0;
  pmdmntInitialize();
  pminfoInitialize();
  pmdmntGetApplicationPid(&pid);
  pminfoGetTitleId(&tid, pid);
  pminfoExit();
  pmdmntExit();
  return tid;
}

bool fileExists (const std::string& name) {
    return (access(name.c_str(), F_OK) != -1);
}

void copy(const char* from, const char* to, bool exfat = false)
{
    //const u64 fat32Max = 0xFFFFFFFF;
    //const u64 splitSize = 0xFFFF0000;
    const u64 smashTID = 0x01006A800016E000;
    // Smaller or larger might give better performance. IDK
    //u64 bufSize = 0x1FFFE000;
    //u64 bufSize = 0x8000000;
    u64 bufSize = 0x0F0F0F0F;

    if(runningTID() != smashTID)
    {
      printf("\nYou must be currently overriding smash for this tool to work");
      return;
    }
    std::ifstream source(from, std::ifstream::binary);
    if(source.fail())
    {
      printf("\ncould not open source file");
      return;
    }
    source.seekg(0, std::ios::end);
    u64 size = source.tellg();
    source.seekg(0);

    if(std::filesystem::space(to).available < size)
    {
      printf("\nNot enough space on sd card.");
      return;
    }
    if(!exfat)
      fsdevCreateFile(to, 0, FS_CREATE_BIG_FILE);

    std::ofstream dest(to, std::ofstream::binary);
    if(dest.fail())
    {
      printf("\ncould not open destination file");
      return;
    }

    char* buf = new char[bufSize];
    u64 sizeWritten = 0;
    int percent = 0;
    //bool FSChecked = false;

    if(size == 0)
      printf("\x1b[4;2HThere might be a problem with the data.arc file on your sd. Please manually Remove it.");
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
      //dest.seekp(source.tellg());
      //dest.seekp(0, std::ios::end);

      source.read(buf, bufSize);
      dest.write(buf, bufSize);
      if(dest.bad())
      {
        printf("\nSomething went wrong!");
        return;
      }
      sizeWritten += bufSize;
      percent = sizeWritten * 100 / size;
      printf("\x1b[10;2H%d/100", percent);
      //printf("\x1b[20;2Hdest pos: %lld, source pos: %lld", (long long int)dest.tellp(), (long long int)source.tellg());  // Debug log
      //printf("\x1b[22;2H%lu/%lu", sizeWritten, size);  // Debug log
      consoleUpdate(NULL);
    }
    delete[] buf;
}

int main(int argc, char **argv)
{
    consoleInit(NULL);

    printf("\nData Arc Dumper");
    printf("\nPress 'A' to dump as a split file.");
    printf("\nPress 'B' to dump as a single file");
    printf("\nPress 'X' to generate an MD5 hash of the file");

    bool done = false;
    bool exfat = false;
    std::string cfw = getCFW();
    std::string outPath = "sdmc:/" + cfw + "/titles/01006A800016E000/romfs/data.arc";

    while(appletMainLoop())
    {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

        if (kDown & KEY_X)
        {
          if(fileExists(outPath))
          {
            printf("\nStarted");
            consoleUpdate(NULL);
            u64 startTime = std::time(0);
            // Should I block home button here too?
            appletSetMediaPlaybackState(true);
            hashwrapper *md5 = new md5wrapper();
            std::string hash = md5->getHashFromFile(outPath);
            delete md5;
            appletSetMediaPlaybackState(false);
            u64 endTime = std::time(0);
            printf("\nmd5:%s\nHashing took %f minutes", hash.c_str(), (float)(endTime - startTime)/60);
          }
          else
          {
            printf("\nNo data.arc found on sd.");
          }
        }
        if (kDown & KEY_B && !done) exfat = true;
        if ((kDown & KEY_A || kDown & KEY_B) && !done)
        {
          printf("\nStarted");
          consoleUpdate(NULL);
          u64 startTime = std::time(0);
          remove(outPath.c_str());
          romfsMountFromCurrentProcess("romfs");
          appletBeginBlockingHomeButton(0);
          appletSetMediaPlaybackState(true);
          copy("romfs:/data.arc", outPath.c_str(), exfat);
          appletEndBlockingHomeButton();
          appletSetMediaPlaybackState(false);
          romfsUnmount("romfs");
          u64 endTime = std::time(0);

          done = true;  // So you don't accidentally dump twice
          printf("\nDone in %f minutes", (float)(endTime - startTime)/60);
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
