#pragma once
#include <switch.h>
extern "C" {
#include "console.h"
}

enum menus {
    mainMenu,
    modInstallerMenu,
    arcDumperMenu,
    ftpMenu
};

menus menu = modInstallerMenu;

void printMainMenu() {
    consoleClear();
    console_set_status("\n" GREEN "Ultimate Mod Manager " VERSION_STRING " " RESET);
    printf("\n\nPress 'A' to go to the Mod Installer");
    printf("\nPress 'X' to go to the Data Arc Dumper");
    printf("\nPress 'Y' to go to the FTP Server");
}
void printDumperMenu() {
    consoleClear();
    console_set_status("\n" GREEN "Data Arc Dumper" RESET);
    printf("\n\nPress 'A' to dump as a split file");
    printf("\nPress 'Y' to dump as a single file (exFAT only)");
    printf("\nPress 'X' to launch smash");
    printf("\nPress 'B' to return to the main menu");
    printf("\nPress 'R'+'X' to generate an MD5 hash of the file\n");
}