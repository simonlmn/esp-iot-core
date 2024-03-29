#ifndef IOT_CORE_SYSTEM_H_
#define IOT_CORE_SYSTEM_H_

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <gpiobj.h>
#include <toolbox.h>
#include "Interfaces.h"
#include "IDateTimeSource.h"
#include "Config.h"
#include "Logger.h"
#include "DateTime.h"
#include "Utils.h"
#include "Version.h"
#include <vector>

// Enable measurement of chip's VCC
ADC_MODE(ADC_VCC);

namespace iot_core {

class System final : public ISystem, public IApplicationContainer {
  static const unsigned long FACTORY_RESET_TRIGGER_TIME = 5000ul; // 5 seconds
  static const unsigned long DISCONNECTED_RESET_TIMEOUT = 300000ul; // 5 minutes
  
  char _chipId[9];
  bool _stopped = false;
  Time _uptime = {};
  unsigned long _disconnectedSinceMs = 1u;
  ConnectionStatus _status = ConnectionStatus::Disconnected;
  const IDateTimeSource* _dateTimeSource = &NO_DATE_TIME_SOURCE;
  LogService _logService;
  Logger _logger;
  WiFiManager _wifiManager {};
  std::vector<IApplicationComponent*> _components {};

  const char* _name;
  const VersionInfo& _version;
  const char* _otaPassword;
  gpiobj::DigitalOutput& _statusLedPin;
  gpiobj::DigitalInput& _otaEnablePin;
  gpiobj::DigitalInput& _updatePin;
  gpiobj::DigitalInput& _factoryResetPin;
  gpiobj::DigitalInput& _debugEnablePin;

  TimingStatistics<20> _yieldTiming {};
  ConstStrMap<TimingStatistics<10>> _componentTiming {};

  std::function<void()> _scheduledFunction {};

public:
  System(const char* name, const VersionInfo& version, const char* otaPassword, gpiobj::DigitalOutput& statusLedPin, gpiobj::DigitalInput& otaEnablePin, gpiobj::DigitalInput& updatePin, gpiobj::DigitalInput& factoryResetPin, gpiobj::DigitalInput& debugEnablePin)
    : _logService(_uptime),
    _logger(_logService.logger("sys")),
    _name(name),
    _version(version),
    _otaPassword(otaPassword),
    _statusLedPin(statusLedPin),
    _otaEnablePin(otaEnablePin),
    _updatePin(updatePin),
    _factoryResetPin(factoryResetPin),
    _debugEnablePin(debugEnablePin)
  {
    toolbox::strref(toolbox::format("%x", ESP.getChipId())).copy(_chipId, 8, true);
  }

  const char* id() const override {
    return _chipId;
  }

  const char* name() const {
    return _name;
  }

  const VersionInfo& version() const override {
    return _version;
  }

  void addComponent(IApplicationComponent* component) override {
    _components.emplace_back(component);
    _componentTiming[component->name()] = {};
  }

  void setup() {
    if (_debugEnablePin) {
      _logService.initialLogLevel(LogLevel::Debug);
    }

#ifdef DEVELOPMENT_MODE
    _logger.log(LogLevel::Warning, "DEVELOPMENT MODE");
#endif
    _statusLedPin = true;

    char hostname[33];
    toolbox::strref(toolbox::format("%s-%s", _name, _chipId)).copy(hostname, std::size(hostname) - 1, true);

    _logger.log(toolbox::format(F("Setting up %s version %s (commit %s)"), name(), version().version_string, version().commit_hash));
    _logger.log(toolbox::format(F("Running on device ID %s"), id()));
    _logger.log(toolbox::format(F("Using hostname %s"), hostname));

    LittleFS.begin();

    _wifiManager.setConfigPortalBlocking(false);
    _wifiManager.setWiFiAutoReconnect(true);
    _wifiManager.setHostname(hostname);
    bool connected = _wifiManager.autoConnect(hostname);

    if (_otaEnablePin) {
      setupOTA();
    }
    
    _logger.log(LogLevel::Info, F("Internal setup done."));

    for (auto component : _components) {
      restoreConfiguration(component);
      component->setup(connected);
    }

    _logger.log(LogLevel::Info, F("All setup done."));
    
    _statusLedPin = false;
  }

  void loop() {
    _yieldTiming.start();

    _uptime.update();

    lyield();

    if (_factoryResetPin && _factoryResetPin.hasNotChangedFor(FACTORY_RESET_TRIGGER_TIME)) {
      factoryReset();
    }

    if (_scheduledFunction) {
      _scheduledFunction();
      _scheduledFunction = nullptr;
    }
    
    if (connected()) {
      _status = ConnectionStatus::Connected;      
      if (_disconnectedSinceMs > 0) {
        _logger.log(LogLevel::Info, toolbox::format(F("Reconnected after %u ms."), _uptime.millis() - _disconnectedSinceMs));
        _disconnectedSinceMs = 0;
        _status = ConnectionStatus::Reconnected;
      }

      if (_stopped) {
        blinkMedium();
      } else {
#ifdef DEVELOPMENT_MODE
        blinkFast();
#else
        _statusLedPin = false;
#endif
        loopComponents();
      }
    } else {
      _status = ConnectionStatus::Disconnected;
      if (_disconnectedSinceMs == 0 || _uptime.millis() < _disconnectedSinceMs /* detect millis() wrap-around */) {
        _disconnectedSinceMs = _uptime.millis();
        _logger.log(LogLevel::Warning, F("Disconnected."));
        _status = ConnectionStatus::Disconnecting;
      }

      if (_wifiManager.getWiFiIsSaved() && _uptime.millis() > _disconnectedSinceMs + DISCONNECTED_RESET_TIMEOUT) {
        reset();
      }
      
      if (_stopped) {
        blinkMedium();
      } else {
        blinkSlow();
      
        loopComponents();
      }
    }

    _yieldTiming.stop();
  }

  void lyield() override {
    _yieldTiming.stop();
    yield();
    _wifiManager.process();
    if (_otaEnablePin) {
      ArduinoOTA.handle();
    }
    yield();
    _yieldTiming.start();
  }

  void reset() override {
    ESP.restart();
  }

  void stop() override {
    if (_stopped) {
      return;
    }
    
    _stopped = true;

    _logger.log(LogLevel::Info, F("STOP!"));
  }

  void factoryReset() override {
    LittleFS.format();
    _wifiManager.erase(true);
    reset();
  }

  bool connected() const override {
    return WiFi.status() == WL_CONNECTED;
  }

  ConnectionStatus connectionStatus() const override {
    return _status;
  }

  DateTime const& currentDateTime() const override {
    return _dateTimeSource->currentDateTime();
  }

  void schedule(std::function<void()> function) override {
    if (_scheduledFunction) {
      auto previousFunction = _scheduledFunction;
      _scheduledFunction = [function,previousFunction] () {
        previousFunction();
        function();
      };
    } else {
      _scheduledFunction = function;
    }
  }

  void setDateTimeSource(const IDateTimeSource* dateTimeSource) {
    _dateTimeSource = dateTimeSource;
  }

  bool configure(const char* category, IConfigParser const& config) override {
    auto component = findComponentByName(category);
    if (component == nullptr) {
      return false;
    }
    if (config.parse([&] (char* name, const char* value) { return component->configure(name, value); })) {
      persistConfiguration(component);
      return true;
    } else {
      return false;
    }
  }

  void getConfig(const char* category, std::function<void(const char*, const char*)> writer) const override {
    auto component = findComponentByName(category);
    if (component == nullptr) {
      return;
    }

    component->getConfig(writer);
  }

  bool configureAll(IConfigParser const& config) override {
    if (config.parse([this] (char* path, const char* value) {
      auto categoryEnd = strchr(path, '.');
      if (categoryEnd == nullptr) {
        return false;
      }
      *categoryEnd = '\0';
      auto category = path;
            
      auto component = findComponentByName(category);
      *categoryEnd = '.';

      if (component == nullptr) {
        return false;
      } else {
        auto name = categoryEnd + 1;
        return component->configure(name, value);
      }
    })) {
      persistAllConfigurations();
      return true;
    } else {
      return false;
    }
  }

  void getAllConfig(std::function<void(const char*, const char*)> writer) const override {
    for (auto component : _components) {
      component->getConfig([&] (const char* name, const char* value) {
        writer(toolbox::format("%s.%s", component->name(), name), value);
      });  
    }
  }

  LogService& logs() override { return _logService; }

  Logger logger(const char* category) override { return _logService.logger(category); }

  void getDiagnostics(IDiagnosticsCollector& collector) const override {
    collector.beginSection("system");
    collector.addValue("chipId", id());
    collector.addValue("flashChipId", toolbox::format("%x", ESP.getFlashChipId()));
    collector.addValue("sketchMD5", ESP.getSketchMD5().c_str());
    collector.addValue("name", name());
    collector.addValue("version", version().version_string);
    collector.addValue("iotCoreVersion", IOT_CORE_VERSION);
    collector.addValue("espCoreVersion", ESP.getCoreVersion().c_str());
    collector.addValue("espSdkVersion", ESP.getSdkVersion());
    collector.addValue("cpuFreq", toolbox::format("%u", ESP.getCpuFreqMHz()));
    collector.addValue("chipVcc", toolbox::format("%1.2f", ESP.getVcc() / 1000.0));
    collector.addValue("resetReason", ESP.getResetReason().c_str());
    collector.addValue("uptime", _uptime.format());
    collector.addValue("freeHeap", toolbox::format("%u", ESP.getFreeHeap()));
    collector.addValue("heapFragmentation", toolbox::format("%u", ESP.getHeapFragmentation()));
    collector.addValue("maxFreeBlockSize", toolbox::format("%u", ESP.getMaxFreeBlockSize()));
    collector.addValue("wifiRssi", toolbox::format("%i", WiFi.RSSI()));
    collector.addValue("ip", WiFi.localIP().toString().c_str());

    collector.beginSection("timing");

    collector.beginSection("yield");
    collector.addValue("count", convert<size_t>::toString(_yieldTiming.count(), 10));
    collector.addValue("avg", convert<unsigned long>::toString(_yieldTiming.avg(), 10));
    collector.addValue("min", convert<unsigned long>::toString(_yieldTiming.min(), 10));
    collector.addValue("max", convert<unsigned long>::toString(_yieldTiming.max(), 10));
    collector.endSection();

    for (auto& [componentName, timing] : _componentTiming) {
      collector.beginSection(componentName);
      collector.addValue("count", convert<size_t>::toString(timing.count(), 10));
      collector.addValue("avg", convert<unsigned long>::toString(timing.avg(), 10));
      collector.addValue("min", convert<unsigned long>::toString(timing.min(), 10));
      collector.addValue("max", convert<unsigned long>::toString(timing.max(), 10));
      collector.endSection();
    }

    collector.endSection();
    collector.endSection();

    for (auto component : _components) {
      collector.beginSection(component->name());
      component->getDiagnostics(collector);
      collector.endSection();
    }
  }

private:
  void loopComponents() {
    for (auto component : _components) {
      auto& timing = _componentTiming[component->name()];
      timing.start();
      component->loop(_status);
      timing.stop();
      lyield();
    }
  }

  IApplicationComponent* findComponentByName(const char* name) {
    for (auto component : _components) {
      if (strcmp(component->name(), name) == 0) {
        return component;
      }
    }
    return nullptr;
  }

  IApplicationComponent const* findComponentByName(const char* name) const {
    for (auto component : _components) {
      if (strcmp(component->name(), name) == 0) {
        return component;
      }
    }
    return nullptr;
  }

  void setupOTA() {
    ArduinoOTA.setPassword(_otaPassword);
    ArduinoOTA.onStart([this] () { stop(); LittleFS.end(); _logger.log(LogLevel::Info, F("Starting OTA update...")); _statusLedPin = true; });
    ArduinoOTA.onEnd([this] () { _statusLedPin = false; _logger.log(LogLevel::Info, F("OTA update finished.")); });
    ArduinoOTA.onProgress([this] (unsigned int /*progress*/, unsigned int /*total*/) { _statusLedPin.trigger(true, 10); });
    ArduinoOTA.begin();
  }

  void blinkSlow() {
    _statusLedPin.toggleIfUnchangedFor(1000ul);
  }

  void blinkMedium() {
    _statusLedPin.toggleIfUnchangedFor(500ul);
  }

  void blinkFast() {
    _statusLedPin.toggleIfUnchangedFor(250ul);
  }

  void restoreConfiguration(IConfigurable* configurable) {
    ConfigParser parser = readConfigFile(toolbox::format("/config/%s", configurable->name()));
    if (parser.parse([&] (char* name, const char* value) { return configurable->configure(name, value); })) {
      _logger.log(LogLevel::Info, toolbox::format(F("Restored config for '%s'."), configurable->name()));
    } else {
      _logger.log(LogLevel::Error, toolbox::format(F("failed to restore config for '%s'."), configurable->name()));
    }
  }

  void persistConfiguration(IConfigurable* configurable) {
    writeConfigFile(toolbox::format("/config/%s", configurable->name()), configurable);
  }

  void persistAllConfigurations() {
    for (auto component : _components) {
      persistConfiguration(component);
    }
  }
};

}

#endif
