#include <MainApplication.hpp>
#include "mod_installer.h"

// Implement all the layout/application functions here

Layout1::Layout1()
{
    // Create the textblock with the text we want
    this->helloText = pu::ui::elm::ScrollableTextBlock::New(0, 0, "", 25, false);
    // Add the textblock to the layout's element container. IMPORTANT! this MUST be done, having them as members is not enough (just a simple way to keep them)
    this->Add(this->helloText);
}

bool selection_finish = false;
bool show_open_main = true;

void MainApplication::openMain(u64 Down, u64 Up, u64 Held, bool Touch)
{
    if (Down & KEY_PLUS) this->CloseWithFadeOut();

    if (!show_open_main) return;

    int opt = this->CreateShowDialog("App Selection", "Select an app to start.", { "Mod Installer", "Data Arc Dumper" }, false);
    switch(opt)
    {
        case 0: // "Yes" was selected
            this->SetOnInput(std::bind(&modInstallerMain, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            break;
        case 1: // "No" was selected
            this->CreateShowDialog("Answer", "Oh... Then I guess you won't have an iPod...", { "(damnit, he caught me)" }, true);
            break;
    }

    show_open_main = false;
}

void MainApplication::modInstallerMain(u64 kDown, u64 kUp, u64 kHeld, bool Touch)
{
    auto textBlock = this->layout1->helloText;
    if (textBlock->prevY != textBlock->GetY()) {
        s32 diff = textBlock->GetY() - textBlock->prevY;
        //for (auto rect : this->layout1->textOverlays)
        //   rect->SetY(rect->GetY() + diff);
    }

    if (!installation_finish && !selection_finish) {
        if (kDown & KEY_B) {
            if(offsetObj != nullptr) {
                delete offsetObj;
                offsetObj = nullptr;
            }
            if(compContext != nullptr) {
                ZSTD_freeCCtx(compContext);
                compContext = nullptr;
            }
            
            this->SetOnInput(std::bind(&openMain, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
            show_open_main = true;
        }

        this->layout1->textOverlays.clear();
        this->layout1->Clear();
        this->layout1->helloText = pu::ui::elm::ScrollableTextBlock::New(0, 0, "", 25, false);
        textBlock = this->layout1->helloText;
        this->layout1->Add(textBlock);
        _printf("Please select a mods folder. Press A to install, Y to uninstall.\n\n");
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
                        _printf("> ");
                        if (start_install) {
                            found_dir = true;
                            add_mod_dir(directory.c_str());
                        }
                    }
                    _printf("%s\n", dir->d_name);
                    curr_folder_index++;
                }
            }

            if (mod_folder_index < 0)
                mod_folder_index = curr_folder_index;

            if (mod_folder_index > curr_folder_index)
                mod_folder_index = 0;

            if (mod_folder_index == curr_folder_index) {
                _printf("> ");
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
            _clr_overlay(PU_RED);
            _printf("%s folder not found\n\n", mods_root);
        }

        
        if (start_install && found_dir) {
            selection_finish = true;
            mkdir(backups_root, 0777);
            perform_installation();
            mod_dirs = NULL;
            num_mod_dirs = 0;
        }
    } else {
        if (kDown & KEY_B) {
            if (installation_finish) {
                installation_finish = false;
                selection_finish = false;
            }
        }
        if(kDown & KEY_X) {
            appletRequestLaunchApplication(0x01006A800016E000, NULL);
        } 
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
    this->SetOnInput(std::bind(&openMain, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
}

// Main entrypoint, call the app here
int main()
{
    // Create a smart pointer of our application (can be a regular instance)
    _main = MainApplication::New();

    // Show -> start rendering in an "infinite" loop
    _main->Show();

    // Exit (Plutonium will handle when the app is closed, and it will be disposed later)
    return 0;
}
