#ifndef IOT_CORE_INTERFACES_H_
#define IOT_CORE_INTERFACES_H_

#include "Logger.h"
#include "DateTime.h"
#include "VersionInfo.h"
#include <toolbox.h>
#include <functional>

namespace iot_core {

enum struct ConnectionStatus {
  Disconnected,
  Reconnected,
  Connected,
  Disconnecting,
};

class ISystem {
public:
  virtual toolbox::strref id() const = 0;
  virtual void reset() = 0;
  virtual void stop() = 0;
  virtual void factoryReset() = 0;
  virtual ConnectionStatus connectionStatus() const = 0;
  virtual bool connected() const = 0;
  virtual LogService& logs() = 0;
  virtual Logger logger(const toolbox::strref& category) = 0;
  virtual ILocalLogSink& localLogSink() = 0;
  virtual void lyield() = 0;
  virtual DateTime const& currentDateTime() const = 0;
  virtual void schedule(std::function<void()> function) = 0;
};

class IDiagnosticsCollector {
public:
  virtual void beginSection(const toolbox::strref& name) = 0;
  virtual void addValue(const toolbox::strref& name, const toolbox::strref& value) = 0;
  virtual void endSection() = 0;
};

class IDiagnosticsProvider {
public:
  virtual void getDiagnostics(IDiagnosticsCollector& collector) const = 0;
};

using ConfigWriter = std::function<void(const toolbox::strref& name, const toolbox::strref& value)>;

struct IConfigurable {
  virtual toolbox::strref name() const = 0;
  virtual bool configure(const toolbox::strref& name, const toolbox::strref& value) = 0;
  virtual void getConfig(ConfigWriter writer) const = 0;
};

class IConfigParser {
public:
  virtual bool parse(std::function<bool(const toolbox::strref& name, const toolbox::strref& value)> processEntry) const = 0;
};

class IApplicationComponent : public IConfigurable, public IDiagnosticsProvider {
public:
  virtual toolbox::strref name() const = 0;
  virtual void setup(bool connected) = 0;
  virtual void loop(ConnectionStatus status) = 0;
};

class IApplicationContainer : public IDiagnosticsProvider {
public:
  virtual const VersionInfo& version() const = 0;
  virtual void addComponent(IApplicationComponent* component) = 0;
  virtual IApplicationComponent const* getComponent(const toolbox::strref& name) const = 0;
  virtual IApplicationComponent* getComponent(const toolbox::strref& name) = 0;
  virtual void forEachComponent(std::function<void(const IApplicationComponent* component)> handler) const = 0;
  virtual bool configure(const toolbox::strref& category, IConfigParser const& config) = 0;
  virtual void getConfig(const toolbox::strref& category, ConfigWriter writer) const = 0;
  virtual bool configureAll(IConfigParser const& config) = 0;
  virtual void getAllConfig(ConfigWriter writer) const = 0;
};

}

#endif
