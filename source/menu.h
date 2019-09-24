#pragma once
#include <switch.h>
extern "C" {
#include "console.h"
}

#define MAIN_MENU 0
#define MOD_INSTALLER_MENU 1
#define ARC_DUMPER_MENU 2
#define FTP_MENU 3

int menu = MAIN_MENU;

void printMainMenu() {
    consoleClear();
    console_set_status("\n" GREEN "Ultimate Mod Manager " VERSION_STRING " " RESET);
    printf("\n\nPress 'A' to go to the Mod Installer");
    printf("\nPress 'X' to go to the Data Arc Dumper");
    printf("\nPress 'Y' to go to the FTP Server");
}