#include <MainApplication.hpp>
#include "mod_installer.h"

// Implement all the layout/application functions here

Layout1::Layout1()
{
    // Create the textblock with the text we want
    this->helloText = pu::ui::elm::ScrollableTextBlock::New(0, 0, "", 25, false);
    // Add the textblock to the layout's element container. IMPORTANT! this MUST be done, having them as members is not enough (just a simple way to keep them)
    this->Add(this->helloText);

    s32 width = 800;
    this->modInstallerMenu = pu::ui::elm::Menu::New(1280 / 2 - (width / 2), 40, width, pu::ui::Color(0,255,0,128), 50, 5);
    this->modInstallerMenu->SetVisible(false);
    this->Add(this->modInstallerMenu);

    this->progressBar = pu::ui::elm::ProgressBar::New(1280 / 2 - (width / 2), 720 / 2, width, 50, 1.0);
    this->progressBar->SetVisible(false);
    this->Add(progressBar);
}

bool selection_finish = false;
bool show_open_main = true;

void MainApplication::openMain(u64 Down, u64 Up, u64 Held, bool Touch)
{
    if (Down & KEY_PLUS) this->Close();

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

void install_mod_dir() {
    installing = INSTALL;
    selection_finish = true;
    add_mod_dir(_main->layout1->modInstallerMenu->GetSelectedItem()->GetName().AsUTF8().c_str());
    mkdir(backups_root, 0777);
    perform_installation();
    mod_dirs = NULL;
    num_mod_dirs = 0;
}

void uninstall_mod_dir() {
    installing = UNINSTALL;
    selection_finish = true;
    add_mod_dir(_main->layout1->modInstallerMenu->GetSelectedItem()->GetName().AsUTF8().c_str());
    mkdir(backups_root, 0777);
    perform_installation();
    mod_dirs = NULL;
    num_mod_dirs = 0;
}

void addMenuItem(std::string name) {
    auto dirMenuItem = pu::ui::elm::MenuItem::New(name);
    dirMenuItem->AddOnClick(&install_mod_dir, KEY_A);
    dirMenuItem->AddOnClick(&uninstall_mod_dir, KEY_Y);
    dirMenuItem->AddOnClick(&install_mod_dir, KEY_TOUCH);
    _main->layout1->modInstallerMenu->AddItem(dirMenuItem);
}

void MainApplication::modInstallerMain(u64 kDown, u64 kUp, u64 kHeld, bool Touch)
{
    if (kDown & KEY_PLUS) this->Close();

    auto textBlock = this->layout1->helloText;

    for (auto rect : this->layout1->textOverlays) {
        if (textBlock->prevY != textBlock->GetY()) {
            s32 diff = textBlock->GetY() - textBlock->prevY;
           rect->SetY(rect->GetY() + diff);
        }

        s32 currY = rect->GetY();
        if(currY <= 720 && currY >= 0)
            this->layout1->Add(rect);
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

<<<<<<< HEAD
        //this->layout1->textOverlays.clear();
        //this->layout1->Clear();
        //this->layout1->helloText = pu::ui::elm::ScrollableTextBlock::New(0, 0, "", 25, false);
        //textBlock = this->layout1->helloText;
        //this->layout1->Add(textBlock);
        this->layout1->helloText->SetText("");
        this->layout1->progressBar->SetProgress(0.0f);
        this->layout1->progressBar->SetVisible(false);
        this->layout1->modInstallerMenu->SetVisible(false);
        this->layout1->modInstallerMenu->ClearItems();
        __printf("Please select a mods folder. Press A to install, Y to uninstall.\n\n");
=======
        this->layout1->textOverlays.clear();
        this->layout1->Clear();
        this->layout1->helloText = pu::ui::elm::ScrollableTextBlock::New(0, 0, "", 25, false);
        textBlock = this->layout1->helloText;
        this->layout1->Add(textBlock);
        _printf("Please select a mods folder. Press A to install, Y to uninstall.\n\n");
>>>>>>> 6898829a1e9aa72e0e4906693255b3e6a98f958c
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

        DIR* d = opendir(mods_root);
        struct dirent *dir;
        if (d) {
            s64 curr_folder_index = 0;
            while ((dir = readdir(d)) != NULL) {
                std::string d_name = std::string(dir->d_name);
                addMenuItem("mods" + d_name);
            }

            closedir(d);
        } else {
            _clr_overlay(PU_RED);
            __printf("%s folder not found\n\n", mods_root);
        }

        addMenuItem("backups");
        this->layout1->modInstallerMenu->SetVisible(true);
        this->layout1->modInstallerMenu->SetNumberOfItemsToShow(this->layout1->modInstallerMenu->GetItems().size());
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
