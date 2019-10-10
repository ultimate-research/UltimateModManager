#pragma once
#include <filesystem>
#include <map>
#include "menu.h"

#define NUM_PROGRESS_CHARS 50
const u64 smashTID = 0x01006A800016E000;
const char* manager_root = "sdmc:/UltimateModManager/";
const char* tablePath = "sdmc:/UltimateModManager/compTable.backup";
enum smashRegions{
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

int getRegion() {
    u64 languageCode;
    //setGetLanguageCode(&languageCode);
    appletGetDesiredLanguage(&languageCode);
    return regionMap.find((char*)&languageCode)->second;
}

void getVersion(u64 tid, char version[0x10]) {
    nsInitialize();
    NsApplicationControlData contolData;
    nsGetApplicationControlData(1, tid, &contolData, sizeof(NsApplicationControlData), NULL);
    nsExit();
    strcpy(version, contolData.nacp.version);
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

bool isServiceRunning(const char *serviceName) {
  Handle handle;
  bool running = R_FAILED(smRegisterService(&handle, serviceName, false, 1));

  svcCloseHandle(handle);

  if (!running)
    smUnregisterService(serviceName);

  return running;
}

std::string getCFW()
{
  if (isServiceRunning("rnx"))
    return "ReiNX";
  if (isServiceRunning("tx"))
    return "sxos";
  return "atmosphere";
}

u64 runningTID()
{
  u64 pid = 0;
  u64 tid = 0;
  pmdmntInitialize();
  pminfoInitialize();
  pmdmntGetApplicationPid(&pid);
  pminfoGetTitleId(&tid, pid);
  pminfoExit();
  pmdmntExit();
  return tid;
}

bool fileExists (const std::string& name) {
    return (access(name.c_str(), F_OK) != -1);
}

int mkdirs (const std::string path, int mode) {
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
  }

  std::filesystem::remove(path);
}
