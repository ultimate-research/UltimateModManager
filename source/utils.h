#pragma once
#include <filesystem>
#include <map>
#include <stack>
#include <stdarg.h>
#include "menu.h"
#include "switch.h"

#define NUM_PROGRESS_CHARS 50
const u64 smashTID = 0x01006A800016E000;
const char* manager_root = "sdmc:/UltimateModManager/";
const char* tablePath = "sdmc:/UltimateModManager/compTable.backup";
bool applicationMode = false;
enum smashRegions {
    jp_ja,
    us_en,
    us_fr,
    us_es,
    eu_en,
    eu_fr,
    eu_es,
    eu_de,
    eu_nl,
    eu_it,
    eu_ru,
    kr_ko,
    zh_cn,
    zh_tw
};
const std::map<std::string, int> regionMap {
    {"ja",      jp_ja},
    {"en-US",   us_en},
    {"fr",      eu_fr},
    {"de",      eu_de},
    {"it",      eu_it},
    {"es",      eu_es},
    {"zh-CN",   zh_cn},
    {"ko",      kr_ko},
    {"nl",      eu_nl},
    {"pt",      eu_en},
    {"ru",      eu_ru},
    {"zh-TW",   zh_tw},
    {"en-GB",   eu_en},
    {"fr-CA",   us_fr},
    {"es-419",  us_es},
    {"zh-Hans", zh_cn},
    {"zh-Hant", zh_tw},
};

const char* log_file = "sdmc:/UltimateModManager/log.txt";
const bool debug = std::filesystem::exists("sdmc:/UltimateModManager/debug.flag");
void debug_log(const char*, ...) __attribute__((format(printf, 1, 2)));
void debug_log(const char* format, ...) {
    if(debug) {
        char buf[10];
        std::time_t now = std::time(0);
        std::strftime(buf, sizeof(buf), "%T", std::localtime(&now));
        va_list args;
        va_start(args, format);
        FILE* log = fopen(log_file, "ab");
        fprintf(log, "[%s] ", buf);
        vfprintf(log, format, args);
        fclose(log);
        va_end(args);
    }
}

u64 getAtmosVersion() {
    splInitialize();
    u64 ver = 0;
    SplConfigItem SplConfigItem_ExosphereVersion = (SplConfigItem)65000;
    if(R_SUCCEEDED(splGetConfig(SplConfigItem_ExosphereVersion, &ver))) {
        u32 major = (ver >> 32) & 0xFF;
        u32 minor = (ver >> 24) & 0xFF;
        u32 micro = (ver >> 16) & 0xFF;
        ver = (major*10000) + (minor*100) + micro;
    }
    splExit();
    return ver;
}

std::string strTolower(std::string string) {
    for(int i = 0; string[i] != 0; i++) {
        string[i] = tolower(string[i]);
    }
    return string;
}

bool isApplicationMode() {
    AppletType currAppType = appletGetAppletType();
    return (currAppType == AppletType_Application || currAppType == AppletType_SystemApplication);
}

int getRegion() {
    u64 languageCode;
    if(R_FAILED(appletGetDesiredLanguage(&languageCode))) {
        if (R_SUCCEEDED(setInitialize())) {
            setGetSystemLanguage(&languageCode);
            setExit();
        }
    }
    return regionMap.find((char*)&languageCode)->second;
}

void getVersion(u64 tid, char version[0x10]) {
    nsInitialize();
    NsApplicationControlData contolData;
    nsGetApplicationControlData(NsApplicationControlSource_Storage, tid, &contolData, sizeof(NsApplicationControlData), NULL);
    nsExit();
    strcpy(version, contolData.nacp.display_version);
}

int getSmashVersion() {
    int out = 0;
    char version[0x10];
    getVersion(smashTID, version);
    char* value = strtok(version, ".");
    out += strtol(value, NULL, 16) * 0x10000;
    value = strtok(NULL, ".");
    out += strtol(value, NULL, 16) * 0x100;
    value = strtok(NULL, ".");
    out += strtol(value, NULL, 16);
    return out;
}

void print_progress(size_t progress, size_t max) {
    size_t prog_chars;
    if (max == 0) prog_chars = NUM_PROGRESS_CHARS;
    else prog_chars = ((float) progress / max) * NUM_PROGRESS_CHARS;

    printf(CONSOLE_ESC(u));
    printf(CONSOLE_ESC(s));
    if (prog_chars < NUM_PROGRESS_CHARS) printf(YELLOW);
    else printf(GREEN);

    printf("[");
    for (size_t i = 0; i < prog_chars; i++)
        printf("=");

    if (prog_chars < NUM_PROGRESS_CHARS) printf(">");
    else printf("=");

    for (size_t i = 0; i < NUM_PROGRESS_CHARS - prog_chars; i++)
        printf(" ");

    printf("]\t%lu/%lu\n" RESET, progress, max);
}

std::string strsprintf(const char*, ...) __attribute__((format(printf, 1, 2)));
std::string strsprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int size = vsnprintf(nullptr, 0, format, args) + 1;
    char* cstr = new char[size];
    vsnprintf(cstr, size, format, args);
    std::string str(cstr);
    delete[] cstr;
    va_end(args);
    return str;
}

std::stack<std::string> errorLogs;
void log(const char*, ...) __attribute__((format(printf, 1, 2)));
void log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(nullptr, 0, format, args) + 1;
    char* buffer = new char[len];
    vsnprintf(buffer, len, format, args);
    errorLogs.emplace(buffer);
    delete[] buffer;
    va_end(args);
}

bool isServiceRunning(const char* serviceName) {
  Handle handle;
  SmServiceName encodedName = smEncodeName(serviceName);
  bool running = R_FAILED(smRegisterService(&handle, encodedName, false, 1));

  svcCloseHandle(handle);

  if (!running)
    smUnregisterService(encodedName);

  return running;
}

enum cfwName {
    atmosphere,
    sxos,
    ReiNX,
};
cfwName getCFW()
{
  if (isServiceRunning("rnx"))
    return ReiNX;
  if (isServiceRunning("tx"))
    return sxos;
  return atmosphere;
}

std::string dataArcPath(cfwName cfw) {
    std::string path;
    switch(cfw) {
        case atmosphere:
            if(getAtmosVersion() >= 1000)
                path = "sdmc:/atmosphere/contents/01006A800016E000/romfs/data.arc";
            else
                path = "sdmc:/atmosphere/titles/01006A800016E000/romfs/data.arc";
            break;
        case sxos:
            path = "sdmc:/sxos/titles/01006A800016E000/romfs/data.arc";
            break;
        case ReiNX:
            path = "sdmc:/ReiNX/titles/01006A800016E000/romfs/data.arc";
            break;
    }
    return path;
}

u64 runningTID()
{
  u64 pid = 0;
  u64 tid = 0;
  pmdmntInitialize();
  pminfoInitialize();
  pmdmntGetApplicationProcessId(&pid);
  pminfoGetProgramId(&tid, pid);
  pminfoExit();
  pmdmntExit();
  return tid;
}

bool fileExists (const std::string& name) {
    return (access(name.c_str(), F_OK) != -1);
}

int mkdirs (const std::string path, int mode = 0777) {
  int slashIDX = path.find_last_of("/");
  if(mkdir(path.c_str(), mode) == -1  && slashIDX != -1) {
    mkdirs(path.substr(0, slashIDX), mode);
    return mkdir(path.c_str(), mode);
  }
  return 0;
}

void vibrateFor(HidVibrationValue VibrationValue, u32 VibrationDeviceHandle[2], s64 time)
{
  // Default values
  HidVibrationValue VibrationValue_stop;
  VibrationValue_stop.amp_low   = 0;
  VibrationValue_stop.freq_low  = 160.0f;
  VibrationValue_stop.amp_high  = 0;
  VibrationValue_stop.freq_high = 320.0f;

  HidVibrationValue VibrationValues[2];
  // ON
  memcpy(&VibrationValues[0], &VibrationValue, sizeof(HidVibrationValue));
  memcpy(&VibrationValues[1], &VibrationValue, sizeof(HidVibrationValue));
  hidSendVibrationValues(VibrationDeviceHandle, VibrationValues, 2);

  svcSleepThread(time);
  // OFF
  memcpy(&VibrationValues[0], &VibrationValue_stop, sizeof(HidVibrationValue));
  memcpy(&VibrationValues[1], &VibrationValue_stop, sizeof(HidVibrationValue));
  hidSendVibrationValues(VibrationDeviceHandle, VibrationValues, 2);
}

void shortVibratePattern()
{
  u32 VibrationDeviceHandle[2];
  HidVibrationValue VibrationValue;

  VibrationValue.amp_low   = 1.0f;
  VibrationValue.freq_low  = 160.0f;
  VibrationValue.amp_high  = 0.2f;
  VibrationValue.freq_high = 320.0f;

  hidInitializeVibrationDevices(VibrationDeviceHandle, 2, CONTROLLER_HANDHELD, (HidControllerType)(TYPE_HANDHELD | TYPE_JOYCON_PAIR));

  vibrateFor(VibrationValue, VibrationDeviceHandle, 1e+9);
  svcSleepThread(3.5e+7);
  vibrateFor(VibrationValue, VibrationDeviceHandle, 3.5e+8);
  svcSleepThread(5e+7);
  vibrateFor(VibrationValue, VibrationDeviceHandle, 3.5e+8);
}

void removeRecursive(std::filesystem::path path)
{
  if (std::filesystem::is_directory(path))
  {
    for (auto & child : std::filesystem::directory_iterator(path))
      removeRecursive(child.path());
    rmdir(path.c_str());
  }

  std::filesystem::remove(path);
}
