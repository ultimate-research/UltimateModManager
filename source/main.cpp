#include <string.h>
#include <stdio.h>
#include <fstream>
#include <sys/stat.h>

#include <switch.h>

//copied from libnx master cause I don't wanna update libnx
Result romfsMountFromCurrentProcess(const char *name) {
    FsStorage storage;

    Result rc = fsOpenDataStorageByCurrentProcess(&storage);
    if (R_FAILED(rc))
        return rc;

    return romfsMountFromStorage(storage, 0, name);
}

void copy(char* from, char* to)
{
    remove(to);
    //rmdir(to);
    appletBeginBlockingHomeButton(0);
    appletSetMediaPlaybackState(true);
    // one eighth of fat32 limit. Smaller might give better performance. IDK
    u64 bufSize = 0x1FFFE000;
    bool exFat = false;
    //bool exists = access( from, F_OK ) != -1;
    // I misunderstood this command
    //fsIsExFatSupported(&exFat);
    if(!exFat)
    {
      //printf("\x1b[40;2HFat32 detected");
      mkdir(to, 777);
      //A file inside is required
      std::fstream file;
      file.open(std::string(to)+"/00",std::ios::out);
      file.close();

      fsdevSetArchiveBit(to);
    }

    std::ifstream source(from, std::ios::binary);
    std::ofstream dest(to, std::ios::binary);

    source.seekg(0, std::ios::end);
    u64 size = source.tellg();
    source.seekg(0);
/*
    struct stat stat_buf;
    stat(to, &stat_buf);
    u64 size = stat_buf.st_size;
*/

    char* buf = new char[bufSize];
    u64 sizeWritten = 0;
    int percent = 0;

    // I don't understand the cause/effect here, but this does seem to solve it.
    if(size == 0)
      printf("\x1b[4;2HThere might be a problem with the data.arc file on your sd. Please manually Remove it.");
    while(sizeWritten < size)
    {
      if(sizeWritten + bufSize > size) //does this matter? Do I want this?
      {
        delete[] buf;
        bufSize = size-sizeWritten;
        buf = new char[bufSize];
      }
      source.read(buf,bufSize);
      dest.write(buf,bufSize);
      sizeWritten += bufSize;
      percent = sizeWritten * 100 / size;
      printf(("\x1b[4;2H" + std::to_string(percent) + "/100").c_str()); // this font not support '%'?
      //printf(("\x1b[35;2H" + std::to_string(size) + ", " + std::to_string(sizeWritten)).c_str());
      consoleUpdate(NULL);
    }
    delete[] buf;
    appletEndBlockingHomeButton();
    appletSetMediaPlaybackState(false);
}

int main(int argc, char **argv)
{
    consoleInit(NULL);

    printf("\x1b[1;2HYou must be currently overriding smash for this tool to work.");
    printf("\x1b[2;2Hpress 'A' to start dumping. This will remove your current data.arc.");
    // Confirm that smash is active process?
    //u64 pid;
    //u64 tid;
    //pmshellGetApplicationPid(&pid);


    bool done = false;
    while(appletMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

        romfsMountFromCurrentProcess("romfs");

        if (kDown & KEY_A && !done)
        {
          printf("\x1b[3;2Hoh boy, here it goes. Give it a minute.");
          consoleUpdate(NULL);
          copy("romfs:/data.arc","/atmosphere/titles/01006A800016E000/romfs/data.arc");
          // did not actually clear the line
          //printf("\x1b[3;2                                        ");
          done = true;  // So you don't accidentally dump twice
          printf("\nDone.");
        }

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}
