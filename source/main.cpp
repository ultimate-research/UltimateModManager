#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <filesystem>
#include <ctime>

#include <switch.h>
#include "dumper.h"
#include "mod_installer.h"
#include "menu.h"
#include "arcReader.h"
extern "C" {
#include "ftp_main.h"
#include "console.h"
}

void mainMenuLoop(int kDown) {
    if (kDown & KEY_A) {
        installation_finish = false;
        menu = MOD_INSTALLER_MENU;

        consoleClear();
        console_set_status("\n" GREEN "Mod Installer" RESET);
    }

    else if (kDown & KEY_X) {
        dump_done = false;
        menu = ARC_DUMPER_MENU;
        printDumperMenu();
    }

    else if (kDown & KEY_Y) {
        menu = FTP_MENU;
    }
}

int main(int argc, char **argv)
{
    console_init();
    console_set_status("\n" GREEN "Mod Installer" RESET);
    if(!std::filesystem::exists(mods_root))
        mkdirs(mods_root, 0777);
    if(!std::filesystem::exists(outPath))
    {
        menu = ARC_DUMPER_MENU;
        printDumperMenu();
    }

    consoleClear();
    arcReader(("sdmc:/" + getCFW() + "/titles/01006A800016E000/romfs/data.arc").c_str());

    while(appletMainLoop())
    {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

        /*
        if (menu == MAIN_MENU)
            mainMenuLoop(kDown);
        else if (menu == MOD_INSTALLER_MENU)
            modInstallerMainLoop(kDown);
        else if (menu == ARC_DUMPER_MENU)
            dumperMainLoop(kDown);
        else if (menu == FTP_MENU)
            ftp_main();
        */

        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}