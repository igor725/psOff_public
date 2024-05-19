#define __APICALL_EXTERN
#include "ipc.h"
#undef __APICALL_EXTERN

#include <Windows.h>

class Communication: public ICommunication {
  private:
  HANDLE                m_hPipe;
  DWORD                 m_dwError;
  std::string           m_pipeName;
  std::vector<PHandler> m_handlers = {};

  bool _iwrite(const void* buffer, size_t size) {
    if (m_hPipe == INVALID_HANDLE_VALUE) {
      m_dwError = ERROR_BROKEN_PIPE;
      return false;
    }

    DWORD cbWrite = 0;
    BOOL  succ;

    succ = WriteFile(m_hPipe, buffer, size, &cbWrite, nullptr);

    if (!succ || size != cbWrite) {
      m_dwError = GetLastError();
      deinit();
      return false;
    }

    return true;
  }

  bool _iread(void* buffer, size_t size) {
    if (m_hPipe == INVALID_HANDLE_VALUE) {
      m_dwError = ERROR_BROKEN_PIPE;
      return false;
    }

    DWORD cbRead = 0;
    BOOL  succ;

    succ = ReadFile(m_hPipe, buffer, size, &cbRead, nullptr);

    if (!succ || cbRead == 0 || cbRead != size) {
      m_dwError = GetLastError();
      deinit();
      return false;
    }

    return true;
  }

  public:
  bool init(const char* name) final {
    if (name)
      m_pipeName = name;
    else
      name = m_pipeName.c_str();

    if (WaitNamedPipeA((LPCSTR)name, 5000)) {
      m_hPipe = CreateFileA((LPCSTR)name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
      if ((m_dwError = GetLastError()) == ERROR_PIPE_BUSY || m_hPipe == INVALID_HANDLE_VALUE) {
        deinit();
        return false;
      }

      DWORD dwMode = PIPE_TYPE_BYTE;

      if (!SetNamedPipeHandleState(m_hPipe, &dwMode, nullptr, nullptr)) {
        m_dwError = GetLastError();
        deinit();
        return false;
      }

      return true;
    }

    m_dwError = GetLastError();
    return false;
  }

  bool deinit() final {
    if (m_hPipe == INVALID_HANDLE_VALUE) return false;
    DisconnectNamedPipe(m_hPipe);
    CloseHandle(m_hPipe);
    m_hPipe = INVALID_HANDLE_VALUE;
    return true;
  }

  bool write(UPackets* packet) final {
    switch (packet->packetId) {
      case PacketID::IPC_PACKET_HANDSHAKE: return _iwrite(&packet->hshake, sizeof(packet->hshake)); break;

      default: return false;
    }
  }

  void runReadLoop() final {
    UPackets packet = {};

    while (_iread(&packet.packetId, sizeof(packet.packetId))) {
      DWORD psize = 0xFFFFFFFF;

      switch (packet.packetId) {
        case PacketID::IPC_PACKET_HANDSHAKE: psize = sizeof(packet.hshake); break;
      }

      if (_iread((char*)&packet + sizeof(packet.packetId), psize - sizeof(packet.packetId))) {
        for (auto handler: m_handlers)
          handler(&packet);
        continue;
      }

      return;
    }
  }
};

ICommunication& accessIPC() {
  static Communication ipc;
  return ipc;
}