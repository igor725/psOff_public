#define __APICALL_EXTERN
#include "gamereport.h"
#undef __APICALL_EXTERN

#include "config_emu.h"
#include "git_ver.h"
#include "logging.h"
#include "modules_include/system_param.h"
#include "third_party/nlohmann/json.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/url.hpp>
#include <format>
#include <shellapi.h>
#include <unordered_map>

LOG_DEFINE_MODULE(GameReport)

namespace {
struct GitHubIssue {
  int         id;
  std::string status;
  std::string descr;
};

enum Langs : uint32_t {
  ENGLISH,
  RUSSIAN,

  LANGS_MAX
};

enum LangString : uint32_t {
  BUTTONS_YES,
  BUTTONS_NO,
  BUTTONS_CANCEL,
  BUTTONS_OK,

  MAIN_TITLE,
  MAIN_USER,
  MAIN_CRASH,

  ISSUE_TEXT,

  ISSUE_STATUS_NOTHING,
  ISSUE_STATUS_INTRO,
  ISSUE_STATUS_INGAME,
  ISSUE_STATUS_PLAYABLE,

  ISSUE_KIND_AUDIO,
  ISSUE_KIND_GRAPHICS,
  ISSUE_KIND_INPUT,
  ISSUE_KIND_SAVEDATA,
  ISSUE_KIND_VIDEO,
  ISSUE_KIND_MISSING_SYMBOL,
  ISSUE_KIND_NVIDIA,

  LOGGING_TITLE,
  LOGGING_TEXT,

  LANG_STR_MAX,
};

static const char* strings[LANGS_MAX][LANG_STR_MAX] = {
    {
        /* Buttons */
        "Yes",
        "No",
        "Cancel",
        "OK",

        /* Main window */
        "File a game report",

        "Do you want to file a report about the game you're currently running?\n"
        "If you click \"Yes\", your default browser will be opened.\n\n"
        "You must have a GitHub account to create an issue!",

        "We believe your emulator just crashed! Do you want to file a game report?\n"
        "If you click \"Yes\", your default browser will be opened.\n\n"
        "You must have a GitHub account to create an issue!",

        /* Issues window */
        "It looks like we already know about the issues in this game!\n\n"
        "Current status: {}\nPossible issues:\n{}\n\n"
        "Do you want to open a game's issue page on GitHub?",

        "nothing (the game don't initialize properly, not loading at all and/or crashing the emulator)",
        "intro (the game display image but don't make it past the menus)",
        "igname (the game can't be either finished, have serious glitches or insufficient performance)",
        "playable (the game can be finished with playable performance and no game breaking glitches)",

        "* audio issues (sound is missing, choppy or played incorrectly);\n",
        "* graphics issues (visual artifacts, low framerate, black screen);\n",
        "* input issues (gamepad/keyboard won't work, input lag is present);\n",
        "* savedata issues (the game does not save your progress or setting);\n",
        "* video issues (ingame videos are decoded incorrectly or are not played at all);\n",
        "* missing symbol (the game needs functions that we haven't implemented yet);\n",
        "* NVIDIA specific (the game has known issues on NVIDIA cards);\n",

        /* Logging warning window */
        "Logging warning",

        "You may not be able to provide log files for the issue you are about to open.\n"
        "Your logger configuration seems to be incorrect, since \"sink\" does not equal \"FileBin\".\n"
        "Creating issues without log file is not recommended.",
    },
    {
        /* Buttons */
        "Да",
        "Нет",
        "Отмена",
        "OK",

        /* Main window */
        "Отправка репорта",

        "Хотите отправить репорт по текущей игре?\n"
        "Если вы нажмёте \"Да\", будет открыт ваш стандартный браузер.\n\n"
        "Вы должны иметь аккаунт на GitHub чтобы создавать репорты!",

        "Похоже, что эмулятор только что крашнулся! Хотите отправить репорт?\n"
        "Если вы нажмёте \"Да\", будет открыт ваш стандартный браузер.\n\n"
        "Вы должны иметь аккаунт на GitHub чтобы создавать репорты!\n",

        /* Issues window */
        "Похоже, что мы уже знаем о проблемах в этой игре!\n\n"
        "Текущий статус: {}\nВозможные проблемы:\n{}\n\n"
        "Хотите открыть страницу игры на GitHub?\n",

        "nothing (игра не инициализируется правильно, не загружается вообще и/или крашит эмулятор)",
        "intro (игра что-то рисует на экране, но уйти дальше главного меню не выходит)",
        "igname (попасть в игру возможно, но пройти её до конца нельзя из-за серьезных багов)",
        "playable (игра может быть полностью завершена без каких-либо серьезных проблем)",

        "* аудио (звук отстает, прерывается или не играет вообще);\n",
        "* графика (артефакты, черный экран, низкий fps);\n",
        "* ввод (геймпад/клавиатура не работают, или присутствует инпут-лаг);\n",
        "* сохранения (the game does not save your progress or setting);\n",
        "* видео (внутриигровые видео декодируются неверно или вообще не воспроизводятся);\n",
        "* нереализовано (этой игре нужны пока что не реализванные эмулятором функции);\n",
        "* NVIDIA-специфика (эта игра имеет проблемы, которые встречаются только на видеокартах NVIDIA);\n",

        /* Logging warning window */
        "Конфигурация логгера",

        "Возможно вы не сможете предоставить лог-файл при создании репорта.\n"
        "Ваша конфигурация логгера может быть неверна, так как \"sink\" не равен \"FileBin\".\n"
        "Создание репортов без логов не рекомендуется.",
    },
};

static const char* get_lang_string(SystemParamLang lang_id, LangString str_id) {
  switch (lang_id) {
    case SystemParamLang::Russian: return strings[Langs::RUSSIAN][str_id];

    default: return strings[Langs::ENGLISH][str_id];
  }
}

static std::unordered_map<std::string, LangString> descrs = {
    {"audio", LangString::ISSUE_KIND_AUDIO},
    {"graphics", LangString::ISSUE_KIND_GRAPHICS},
    {"input", LangString::ISSUE_KIND_INPUT},
    {"savedata", LangString::ISSUE_KIND_SAVEDATA},
    {"video", LangString::ISSUE_KIND_VIDEO},
    {"missing-symbol", LangString::ISSUE_KIND_MISSING_SYMBOL},
    {"nvidia-specific", LangString::ISSUE_KIND_NVIDIA},
};

static std::unordered_map<std::string, LangString> statuses = {
    {"status-nothing", LangString::ISSUE_STATUS_NOTHING},
    {"status-intro", LangString::ISSUE_STATUS_INTRO},
    {"status-ingame", LangString::ISSUE_STATUS_INGAME},
    {"status-playable", LangString::ISSUE_STATUS_PLAYABLE},
};

static const char* retrieve_label_description(SystemParamLang lang_id, std::string& label) {
  auto it = descrs.find(label);
  if (it != descrs.end()) return get_lang_string(lang_id, it->second);
  return nullptr;
}

static const char* retrieve_label_status(SystemParamLang lang_id, std::string& label) {
  auto it = statuses.find(label);
  if (it != statuses.end()) return get_lang_string(lang_id, it->second);
  return nullptr;
};

static int find_issue(SystemParamLang lang_id, const char* title_id, GitHubIssue* issue) {
  LOG_USE_MODULE(GameReport);
  using namespace boost::asio;
  using namespace boost::beast;
  using json = nlohmann::json;

  boost::urls::url link("https://api.github.com/search/issues");

  auto params = link.params();
  params.append({"q", std::format("repo:{} is:issue {} in:title", GAMEREPORT_REPO_NAME, title_id)});
  link.normalize();

  io_service svc;

  ssl::context                 ctx(ssl::context::sslv23_client);
  ssl::stream<ip::tcp::socket> sock = {svc, ctx};
  try {
    ip::tcp::resolver        resolver(svc);
    ip::tcp::resolver::query query(ip::tcp::v4(), link.host(), link.scheme());
    boost::asio::connect(sock.lowest_layer(), resolver.resolve(query));
    sock.handshake(ssl::stream_base::handshake_type::client);
  } catch (boost::system::system_error& ex) {
    LOG_ERR(L"%S", ex.what());
    return -1;
  }

  http::request<http::empty_body> req {http::verb::get, link, 11};
  req.set(http::field::host, link.host());
  req.set(http::field::user_agent, "psOff-http/1.0");
  req.set("X-GitHub-Api-Version", "2022-11-28");
  // req.set(http::field::authorization, "Bearer @GIT_TOKEN@");
  http::write(sock, req);

  http::response<http::string_body> res;
  flat_buffer                       buf;
  http::read(sock, buf, res);

  if (res[http::field::content_type].contains("application/json") && res.result_int() == 200) {
    try {
      json jresp = json::parse(res.body());
      if (jresp["total_count"] < 1) return -1;
      auto& jissue  = jresp["items"][0];
      auto& jlabels = jissue["labels"];

      std::string tempstr;
      for (auto it: jlabels) {
        it["name"].get_to(tempstr);

        if (auto gstat = retrieve_label_status(lang_id, tempstr)) issue->status = gstat;
        if (auto descr = retrieve_label_description(lang_id, tempstr)) issue->descr += descr;
      }
      if (auto len = issue->descr.length()) issue->descr.erase(len - 2);
      jissue["number"].get_to(issue->id);
    } catch (json::exception& ex) {
      LOG_ERR(L"find_issue => jsonparse error: %S", ex.what());
      return -1;
    }
  } else {
    LOG_ERR(L"GitHub request error: %S", res.body().c_str());
    return -1;
  }

  return 0;
}
} // namespace

class GameReport: public IGameReport {
  public:
  GameReport();
  virtual ~GameReport() = default;

  virtual void ShowReportWindow(const Info& info) final;
};

GameReport::GameReport() {
  // for (int i = 0; i < LANG_STR_MAX; i++) {
  //   printf("%s\n = \n%s\n*************************\n", strings[Langs::ENGLISH][i], strings[Langs::RUSSIAN][i]);
  // }
}

void GameReport::ShowReportWindow(const Info& info) {
  LOG_USE_MODULE(GameReport);
  if (m_bEnabled == false) {
    LOG_WARN(L"[gamereport] Reporting is disabled!");
    return;
  }

  SystemParamLang lang_id;

  {
    auto [lock, jData] = accessConfig()->accessModule(ConfigModFlag::GENERAL);
    if (!getJsonParam(jData, "systemlang", lang_id)) lang_id = SystemParamLang::EnglishUS;
  }

  const char* message;

  switch (info.type) {
    case USER: message = get_lang_string(lang_id, LangString::MAIN_USER); break;

    case EXCEPTION:
    case MISSING_SYMBOL: message = get_lang_string(lang_id, LangString::MAIN_CRASH); break;

    default: break;
  }

  SDL_MessageBoxButtonData btns[2] {
      {.flags = 0, .buttonid = 1, .text = get_lang_string(lang_id, LangString::BUTTONS_YES)},
      {.flags = 0, .buttonid = 2, .text = get_lang_string(lang_id, LangString::BUTTONS_NO)},
  };

  SDL_MessageBoxData mbd {
      .flags      = SDL_MESSAGEBOX_INFORMATION,
      .window     = info.wnd,
      .title      = get_lang_string(lang_id, LangString::MAIN_TITLE),
      .message    = message,
      .numbuttons = 2,
      .buttons    = btns,
  };

  int btn = -1;

  if (SDL_ShowMessageBox(&mbd, &btn) != 0) {
    LOG_ERR(L"[gamereport] Failed to generate SDL MessageBox: %S", SDL_GetError());
    return;
  }

  if (btn != 1) return;

  GitHubIssue issue {
      .id = -1,
  };

  std::string issue_msg;

  if (find_issue(lang_id, info.title_id, &issue) == 0) {
    issue_msg   = std::vformat(get_lang_string(lang_id, LangString::ISSUE_TEXT), std::make_format_args(issue.status, issue.descr));
    mbd.message = issue_msg.c_str();

    if (SDL_ShowMessageBox(&mbd, &btn) != 0) {
      LOG_ERR(L"[gamereport] Failed to generate SDL MessageBox: %S", SDL_GetError());
      return;
    }

    if (btn != 1) return;
  }

  try {
    std::string str;
    auto [lock, jData] = accessConfig()->accessModule(ConfigModFlag::LOGGING);
    (*jData)["sink"].get_to(str);
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });

    if (str != "filebin") {
      mbd.title      = get_lang_string(lang_id, LangString::LOGGING_TITLE);
      mbd.message    = get_lang_string(lang_id, LangString::LOGGING_TEXT);
      mbd.flags      = SDL_MESSAGEBOX_WARNING;
      mbd.numbuttons = 2;
      btns[0].text   = get_lang_string(lang_id, LangString::BUTTONS_OK);
      btns[1].text   = get_lang_string(lang_id, LangString::BUTTONS_CANCEL);

      if (SDL_ShowMessageBox(&mbd, &btn) != 0) {
        LOG_ERR(L"[gamereport] Failed to generate SDL MessageBox: %S", SDL_GetError());
        return;
      }

      if (btn != 1) return;
    }
  } catch (const json::exception& ex) {
    LOG_ERR(L"Something off with the logging configuration: %S", ex.what());
  }

  boost::urls::url link("https://github.com/");

  if (issue.id == -1) {
    link.set_path(std::format("/{}/issues/new", GAMEREPORT_REPO_NAME));
    auto params = link.params();
    params.append({"template", "game_report.yml"});
    switch (info.type) {
      case EXCEPTION:
        params.append({"what-happened", info.add.ex->what()}); // todo: add stack trace?
        break;
      case MISSING_SYMBOL:
        params.append({"what-happened", info.add.message}); // todo: some formatting?
        break;

      default: break;
    }
    params.append({"title", std::format("[{}]: {}", info.title_id, info.title)});
    params.append({"game-version", info.app_ver});
    params.append({"lib-version", git::CommitSHA1().data()});
  } else {
    link.set_path(std::format("/{}/issues/{}", GAMEREPORT_REPO_NAME, issue.id));
  }

  ShellExecuteA(nullptr, nullptr, link.normalize().c_str(), nullptr, nullptr, SW_SHOW);
}

IGameReport& accessGameReport() {
  static GameReport obj;
  return obj;
}