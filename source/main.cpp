#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <filesystem>
#include <ctime>

#include <switch.h>
#include "dumper.h"
#include "mod_installer.h"
#include "menu.h"
#include "utils.h"
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
        nifmInitialize();
        NifmInternetConnectionStatus connectionStatus;
        if(R_SUCCEEDED(nifmGetInternetConnectionStatus(nullptr, nullptr, &connectionStatus))) {
            if(connectionStatus == NifmInternetConnectionStatus_Connected)
                menu = FTP_MENU;
        }
        nifmExit();
    }
}

int main(int argc, char **argv)
{
    console_init();
    console_set_status("\n" GREEN "Mod Installer" RESET);
    if(!std::filesystem::exists(mods_root))
        mkdirs(mods_root, 0777);
    if(std::filesystem::is_directory(std::filesystem::status(arc_path)) && !std::filesystem::is_empty(arc_path))
        fsdevSetArchiveBit(arc_path.c_str());
    if(!std::filesystem::exists(outPath) || std::filesystem::is_empty(arc_path))
    {
        menu = ARC_DUMPER_MENU;
        printDumperMenu();
    }
    smashVersion = getSmashVersion();
    applicationMode = isApplicationMode();

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
    console_exit();
    return 0;
}