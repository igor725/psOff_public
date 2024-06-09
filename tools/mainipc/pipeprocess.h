#pragma once

#include <functional>
#include <mutex>
#include <vector>
#include <windows.h>

struct IPCHeader;

struct PipeProcess {
  bool              closed;
  HANDLE            hPipe;
  DWORD             procId;
  std::vector<char> vWriteData;

  std::function<void(PipeProcess* pproc, IPCHeader* phead)> reader;

  std::mutex wrMutex;

  PipeProcess(HANDLE pipe): hPipe(pipe), vWriteData() {}
};

PipeProcess* CreatePipedProcess(const char* procpath, const char* addarg, const char* pipeid);
