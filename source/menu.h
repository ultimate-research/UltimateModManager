#pragma once
#include <switch.h>
extern "C" {
#include "console.h"
}

#define MAIN_MENU 0
#define MOD_INSTALLER_MENU 1
#define ARC_DUMPER_MENU 2
#define FTP_MENU 3

int menu = MOD_INSTALLER_MENU;

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
    printf("\n\nPress 'A' to dump as a split file (FAT32)");
    printf("\nPress 'Y' to dump as a single file (exFAT)");
    printf("\nPress 'X' to launch smash");
    printf("\nPress 'B' to return to the main menu");
    printf("\nPress 'R'+'X' to generate an MD5 hash of the file\n");
}