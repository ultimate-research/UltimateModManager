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

        // clear screen and home cursor
        printf( CONSOLE_ESC(2J) );
        printf("\n\x1b[1;32mMod Installer\x1b[0m");
    }

    else if (kDown & KEY_X) {
        menu = ARC_DUMPER_MENU;

        // clear screen and home cursor
        printf( CONSOLE_ESC(2J) );
        printf("\n\x1b[1;32mData Arc Dumper\x1b[0m");
        printf("\n\nPress 'A' to dump as a split file (FAT32)");
        printf("\nPress 'B' to dump as a single file (exFAT)");
        printf("\nPress 'X' to generate an MD5 hash of the file");
    }
}

int main(int argc, char **argv)
{
    consoleInit(NULL);
    printf("\n\x1b[1;32mUltimate Mod Manager\x1b[0m");
    printf("\n\nPress 'A' to go to the Mod Installer");
    printf("\nPress 'X' to go to the Data Arc Dumper");

    while(appletMainLoop())
    {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

        switch (menu) {
            case MAIN_MENU:
                mainMenuLoop(kDown);
                break;
            case MOD_INSTALLER_MENU:
                modInstallerMainLoop(kDown);
                break;
            case ARC_DUMPER_MENU:
                dumperMainLoop(kDown);
                break;
        }

        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
