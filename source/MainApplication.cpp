#include <MainApplication.hpp>
#include "mod_installer.h"
#include "ScrollableTextBlock.hpp"

// Implement all the layout/application functions here

Layout1::Layout1()
{
    // Create the textblock with the text we want
    this->helloText = pu::ui::elm::ScrollableTextBlock::New(0, 0, "Press X to answer my question.");
    // Add the textblock to the layout's element container. IMPORTANT! this MUST be done, having them as members is not enough (just a simple way to keep them)
    this->Add(this->helloText);
}

void MainApplication::modInstallerMain(u64 kDown, u64 kUp, u64 kHeld, bool Touch)
{
    if (!installation_finish) {
        _main->layout1->helloText->SetText("");
        if (kHeld & KEY_RSTICK_DOWN) {
            svcSleepThread(7e+7);
            mod_folder_index++;
        }
        if (kHeld & KEY_RSTICK_UP) {
            svcSleepThread(7e+7);
            mod_folder_index--;
        }
        if (kDown & KEY_DDOWN || kDown & KEY_LSTICK_DOWN)
            mod_folder_index++;
        else if (kDown & KEY_DUP || kDown & KEY_LSTICK_UP)
            mod_folder_index--;

        bool start_install = false;
        if (kDown & KEY_A) {
            start_install = true;
            installing = INSTALL;
        }
        else if (kDown & KEY_Y) {
            start_install = true;
            installing = UNINSTALL;
        }
        bool found_dir = false;

        _printf("Please select a mods folder. Press A to install, Y to uninstall.\n\n");

        DIR* d = opendir(mods_root);
        struct dirent *dir;
        if (d) {
            s64 curr_folder_index = 0;
            while ((dir = readdir(d)) != NULL) {
                std::string d_name = std::string(dir->d_name);
                if(dir->d_type == DT_DIR) {
                    if (d_name == "." || d_name == "..")
                        continue;
                    std::string directory = "mods/" + d_name;

                    if (curr_folder_index == mod_folder_index) {
                        _printf("%s> ", CONSOLE_GREEN);
                        if (start_install) {
                            found_dir = true;
                            add_mod_dir(directory.c_str());
                        }
                    }
                    _printf("%s\n", dir->d_name);
                    if (curr_folder_index == mod_folder_index)
                        _printf(CONSOLE_RESET);
                    curr_folder_index++;
                }
            }

            if (mod_folder_index < 0)
                mod_folder_index = curr_folder_index;

            if (mod_folder_index > curr_folder_index)
                mod_folder_index = 0;

            if (mod_folder_index == curr_folder_index) {
                _printf(CONSOLE_GREEN "> ");
                if (start_install) {
                    found_dir = true;
                    add_mod_dir("backups");
                }
            }

            _printf("backups\n");

            if (mod_folder_index == curr_folder_index)
                _printf(CONSOLE_RESET);

            closedir(d);
        } else {
            _printf(CONSOLE_RED "%s folder not found\n\n" CONSOLE_RESET, mods_root);
        }

        
        if (start_install && found_dir) {
            mkdir(backups_root, 0777);
            perform_installation();
            mod_dirs = NULL;
            num_mod_dirs = 0;
        }
    }

    if (kDown & KEY_B) {
        if (installation_finish) {
            installation_finish = false;
        }
        else {
            if(offsetObj != nullptr) {
                delete offsetObj;
                offsetObj = nullptr;
            }
            if(compContext != nullptr) {
                ZSTD_freeCCtx(compContext);
                compContext = nullptr;
            }
            menu = MAIN_MENU;
        }
    }
    if(kDown & KEY_X) {
        appletRequestLaunchApplication(0x01006A800016E000, NULL);
    }
}

MainApplication::MainApplication()
{
    // Create the layout (calling the constructor above)
    this->layout1 = Layout1::New();
    // Load the layout. In applications layouts are loaded, not added into a container (you don't select a added layout, just load it from this function)
    this->LoadLayout(this->layout1);
    // Set a function when input is caught. This input handling will be the first one to be handled (before Layout or any Elements)
    // Using a lambda function here to simplify things
    this->SetOnInput([&](u64 Down, u64 Up, u64 Held, bool Touch)
    {
        if(Down & KEY_X) // If A is pressed, start with our dialog questions!
        {
            int opt = this->CreateShowDialog("App Selection", "Select an app to start.", { "Mod Installer", "Data Arc Dumper", "FTP Server", "Cancel" }, true); // (using latest option as cancel option)
            if((opt == -1) || (opt == -2)) // -1 and -2 are similar, but if the user cancels manually -1 is set, other types or cancel should be -2.
            {
                this->CreateShowDialog("Cancel", "Last question was canceled.", { "Ok" }, true);
            }
            else
            {
                switch(opt)
                {
                    case 0: // "Yes" was selected
                        this->SetOnInput(std::bind(&modInstallerMain, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
                        break;
                    case 1: // "No" was selected
                        this->CreateShowDialog("Answer", "Oh... Then I guess you won't have an iPod...", { "(damnit, he caught me)" }, true);
                        break;
                    case 2:
                        this->layout1->helloText->SetText(this->layout1->helloText->GetText() + "\nYou selected the FTP Server! :D");
                        break;
                }
            }
        }
    });
}

// Main entrypoint, call the app here
int main()
{
    // Create a smart pointer of our application (can be a regular instance)
    _main = MainApplication::New();

    // Show -> start rendering in an "infinite" loop
    _main->Show();

    while (appletMainLoop()) {
        hidScanInput();

        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu
    }

    // Exit (Plutonium will handle when the app is closed, and it will be disposed later)
    return 0;
}