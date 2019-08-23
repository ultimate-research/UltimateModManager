#pragma once
#include <filesystem>
#include "menu.h"

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
