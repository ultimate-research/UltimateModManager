#pragma once
#include <filesystem>
#include <switch.h>

#define MAIN_MENU 0
#define MOD_INSTALLER_MENU 1
#define ARC_DUMPER_MENU 2

int menu = MAIN_MENU;

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
  if (isServiceRunning("tx"))
    return "sxos";
  else if (isServiceRunning("rnx"))
    return "reinx";
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