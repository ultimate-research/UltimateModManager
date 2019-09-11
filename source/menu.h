#pragma once
#include <switch.h>

#define MAIN_MENU 0
#define MOD_INSTALLER_MENU 1
#define ARC_DUMPER_MENU 2
#define FTP_MENU 3

int menu = MAIN_MENU;

// void printMainMenu() {
//     consoleClear();
//     printf("\n" CONSOLE_GREEN "Ultimate Mod Manager" CONSOLE_RESET);
//     printf("\n\nPress 'Y' to go to the Mod Installer");
//     printf("\nPress 'X' to go to the Data Arc Dumper");
//     printf("\nPress 'A' to go to the FTP Server");
// }