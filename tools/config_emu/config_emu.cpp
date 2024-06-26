#define __APICALL_EXTERN
#include "config_emu.h"
#undef __APICALL_EXTERN

#include <fstream>
#include <future>

namespace {
class Item {
  public:
  std::string_view const _name;

  json              _json;
  boost::mutex      _mutex;
  std::future<void> _future;

  Item(std::string_view name): _name(name) {}

  std::pair<boost::unique_lock<boost::mutex>, json*> access() {
    _future.wait();
    return std::make_pair(boost::unique_lock(_mutex), &_json);
  }

  bool save() {
    auto path = std::string("./config/") + _name.data();
    try {
      std::ofstream json_file(path);
      json_file << std::setw(2) << _json;
      return true;
    } catch (const json::exception& e) {
      printf("Failed to save %s: %s\n", _name.data(), e.what());
      return false;
    }
  };
};
} // namespace

class Config: public IConfig {

  Item m_logging  = {"logging.json"};
  Item m_graphics = {"graphics.json"};
  Item m_audio    = {"audio.json"};
  Item m_controls = {"controls.json"};
  Item m_general  = {"general.json"};

  public:
  Config();
  virtual ~Config() = default;

  std::pair<boost::unique_lock<boost::mutex>, json*> accessModule(ConfigModFlag flag);

  virtual bool save(uint32_t flags);
};

IConfig* accessConfig() {
  static Config obj;
  return &obj;
}

std::pair<boost::unique_lock<boost::mutex>, json*> Config::accessModule(ConfigModFlag flag) {
  switch (flag) {
    case ConfigModFlag::LOGGING: return m_logging.access();
    case ConfigModFlag::GRAPHICS: return m_graphics.access();
    case ConfigModFlag::AUDIO: return m_audio.access();
    case ConfigModFlag::CONTROLS: return m_controls.access();
    case ConfigModFlag::GENERAL: return m_general.access();

    default: printf("Invalid bit flag!\n"); exit(1);
  }
}

bool Config::save(uint32_t flags) {

  bool result = true;

  if (!std::filesystem::is_directory("./config/")) std::filesystem::create_directory("./config/");
  if (flags & (uint32_t)ConfigModFlag::LOGGING) result &= m_logging.save();
  if (flags & (uint32_t)ConfigModFlag::GRAPHICS) result &= m_graphics.save();
  if (flags & (uint32_t)ConfigModFlag::AUDIO) result &= m_audio.save();
  if (flags & (uint32_t)ConfigModFlag::CONTROLS) result &= m_controls.save();
  if (flags & (uint32_t)ConfigModFlag::GENERAL) result &= m_general.save();

  return true;
}

Config::Config() {
  auto load = [this](Item* item, json defaults = {}, ConfigModFlag dflag = ConfigModFlag::NONE) {
    auto path          = std::string("./config/") + item->_name.data();
    bool should_resave = false, should_backup = false;

    try {
      std::ifstream json_file(path);
      item->_json = json::parse(json_file, nullptr, true, true);

      if ((item->_json).type() == defaults.type()) {
        printf("Config %s loaded successfully\n", item->_name.data());
      } else {
        should_backup = true;
        should_resave = true;
      }
    } catch (const json::exception& e) {
      if ((std::filesystem::exists(path))) printf("Failed to parse %s: %s\n", item->_name.data(), e.what());

      item->_json   = defaults;
      should_resave = true;
      should_backup = true;
    }

    for (auto& [dkey, dval]: defaults.items()) {
      if ((item->_json)[dkey].is_null() && !dval.is_null()) {
        (item->_json)[dkey] = dval;
        should_resave       = true;
        printf("%s: missing parameter \"%s\" has been added!\n", item->_name.data(), dkey.c_str());
      }
    }

    for (auto& [ckey, cval]: item->_json.items()) {
      if (defaults[ckey].is_null()) {
        item->_json.erase(ckey);
        should_backup = true;
        should_resave = true;
        printf("%s: unused parameter \"%s\" has been removed!\n", item->_name.data(), ckey.c_str());
      }
    }

    if (should_backup == true && std::filesystem::exists(path)) {
      try {
        std::filesystem::path newp(path);
        newp.replace_extension(".back");
        std::filesystem::rename(path, newp);
      } catch (const std::filesystem::filesystem_error& e) {
        printf("%s: failed to create a backup: %s", path.c_str(), e.what());
      }
    }

    if (should_resave && dflag != ConfigModFlag::NONE) this->save((uint32_t)dflag);
  };

  const json defaultpad = {{"type", "gamepad"}, {"deadzones", {{"left_stick", {{"x", 0.0f}, {"y", 0.0f}}}, {"right_stick", {{"x", 0.0f}, {"y", 0.0f}}}}}};

  m_logging._future = std::async(std::launch::async, load, &m_logging, json({{"sink", "FileBin"}, {"verbosity", 1}}), ConfigModFlag::LOGGING);

  m_graphics._future = std::async(std::launch::async, load, &m_graphics, json::object({}), ConfigModFlag::GRAPHICS);

  m_audio._future = std::async(std::launch::async, load, &m_audio, json({{"volume", 0.5f}, {"device", "[default]"}}), ConfigModFlag::AUDIO);

  m_controls._future = std::async(std::launch::async, load, &m_controls,
                                  json({{"pads", json::array({defaultpad, defaultpad, defaultpad, defaultpad})},
                                        {"keybinds",
                                         {
                                             {"triangle", "i"}, {"square", "j"},       {"circle", "l"},       {"cross", "k"},
                                             {"dpad_up", "up"}, {"dpad_down", "down"}, {"dpad_left", "left"}, {"dpad_right", "right"},
                                             {"options", "f1"}, {"touchpad", "f4"},    {"l1", "f3"},          {"l2", "f5"},
                                             {"l3", "space"},   {"r1", "f2"},          {"r2", "f6"},          {"r3", "home"},
                                             {"lx-", "a"},      {"lx+", "d"},          {"ly-", "w"},          {"ly+", "s"},
                                             {"rx-", "h"},      {"rx+", "f"},          {"ry-", "t"},          {"ry+", "g"},
                                         }}}),
                                  ConfigModFlag::CONTROLS);

  m_general._future = std::async(std::launch::async, load, &m_general, json({{"systemlang", 1}}), ConfigModFlag::GENERAL);
}
