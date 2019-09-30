#pragma once
#include <filesystem>
#include <experimental/filesystem>
#include "menu.h"

#define NUM_PROGRESS_CHARS 50

void print_progress(size_t progress, size_t max, std::string currModFile = "") {
    size_t prog_chars;
    if (max == 0) prog_chars = NUM_PROGRESS_CHARS;
    else prog_chars = ((float) progress / max) * NUM_PROGRESS_CHARS;

    printf(CONSOLE_ESC(u) CONSOLE_ESC(s));
    if (prog_chars < NUM_PROGRESS_CHARS) printf(YELLOW);
    else printf(GREEN);

    printf("[");
    for (size_t i = 0; i < prog_chars; i++)
        printf("=");

    if (prog_chars < NUM_PROGRESS_CHARS) printf(">");
    else printf("=");

    for (size_t i = 0; i < NUM_PROGRESS_CHARS - prog_chars; i++)
        printf(" ");

    printf("]\n" RESET);

    if (currModFile == "") printf(CONSOLE_ESC(K));
    else printf(YELLOW "%s\n" RESET, currModFile.c_str());
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
