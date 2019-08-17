#include <string.h>
#include <stdio.h>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <ctime>

#include <switch.h>
#include "dumper.h"
#include "mod_installer.h"

void mainMenuLoop(int kDown) {
    if (kDown & KEY_A) {
        menu = MOD_INSTALLER_MENU;

        consoleClear();
        printf("\n\x1b[1;32mMod Installer\x1b[0m\n");

        if (installation_finish)
            printf("Mod Installer already finished. Press B to return to the main menu.\n\n");
    }

    else if (kDown & KEY_X) {
        menu = ARC_DUMPER_MENU;

        consoleClear();
        printf("\n\x1b[1;32mData Arc Dumper\x1b[0m");
        printf("\n\nPress 'A' to dump as a split file (FAT32)");
        printf("\nPress 'B' to dump as a single file (exFAT)");
        printf("\nPress 'X' to generate an MD5 hash of the file");
    }
}

int main(int argc, char **argv)
{
    consoleInit(NULL);
    printMainMenu();

    while(appletMainLoop())
    {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

        if (menu == MAIN_MENU)
            mainMenuLoop(kDown);
        else if (menu == MOD_INSTALLER_MENU)
            modInstallerMainLoop(kDown);
        else if (menu == ARC_DUMPER_MENU)
            dumperMainLoop(kDown);

        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
