#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <filesystem>
#include <ctime>

#include <switch.h>
#include "dumper.h"
#include "mod_installer.h"
extern "C" {
#include "ftp_main.h"
#include "console.h"
}

void mainMenuLoop(int kDown) {
    if (kDown & KEY_Y) {
        installation_finish = false;
        menu = MOD_INSTALLER_MENU;

        consoleClear();
        console_set_status("\n" GREEN "Mod Installer" RESET);

        if (installation_finish)
            printf("Mod Installer already finished. Press B to return to the main menu.\n\n");
    }

    else if (kDown & KEY_X) {
        dump_done = false;
        menu = ARC_DUMPER_MENU;

        consoleClear();
        console_set_status("\n" GREEN "Data Arc Dumper" RESET);
        printf("\n\nPress 'A' to dump as a split file (FAT32)");
        printf("\nPress 'Y' to dump as a single file (exFAT)");
        printf("\nPress 'X' to generate an MD5 hash of the file");
        printf("\nPress 'B' to return to the main menu");
    }

    else if (kDown & KEY_A) {
        menu = FTP_MENU;
    }
}

int main(int argc, char **argv)
{
    console_init();
    printMainMenu();
    if(!std::filesystem::exists(mods_root))
        mkdirs(mods_root, 0777);

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
        else if (menu == FTP_MENU)
            ftp_main();

        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}