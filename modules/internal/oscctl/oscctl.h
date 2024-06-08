#pragma once

#include "external/libSceIme/types.h"
#include "utility/utility.h"

#include <string>

class IOscCtl {
  CLASS_NO_COPY(IOscCtl);
  CLASS_NO_MOVE(IOscCtl);

  public:
  enum class Status {
    NONE,
    HIDDEN,
    SHOWN,
    DONE,
    CANCELLED,
  };

  struct Params {
    Status      status     = Status::NONE;
    std::string details    = "";
    std::string enterLabel = "";

    char*    internal_buffer = nullptr;
    wchar_t* buffer          = nullptr;
    size_t   buffersz        = 0;

    float xpos    = 0.0f;
    float ypos    = 0.0f;
    bool  aborted = false;
  };

  IOscCtl()          = default;
  virtual ~IOscCtl() = default;

  virtual Params* getParams() = 0;

  virtual bool setEnterLabel(SceImeEnterLabel label)   = 0;
  virtual bool setBuffer(wchar_t* buff, size_t buffsz) = 0;

  virtual bool cancel() = 0;
  virtual bool enter()  = 0;

  virtual bool run(const wchar_t* title) = 0;
  virtual bool show()                    = 0;
  virtual void destroy()                 = 0;
  virtual void abort()                   = 0;
};

#if defined(__APICALL_EXTERN)
#define __APICALL __declspec(dllexport)
#elif defined(__APICALL_IMPORT)
#define __APICALL __declspec(dllimport)
#else
#define __APICALL
#endif
__APICALL IOscCtl& accessOscCtl();
#undef __APICALL
