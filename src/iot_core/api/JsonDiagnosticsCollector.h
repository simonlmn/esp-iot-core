#ifndef IOT_CORE_API_JSONDIAGNOSTICSCOLLECTOR_H_
#define IOT_CORE_API_JSONDIAGNOSTICSCOLLECTOR_H_

#include <iot_core/Interfaces.h>
#include <jsons/Writer.h>

namespace iot_core::api {

class JsonDiagnosticsCollector final : public iot_core::IDiagnosticsCollector {
private:
  jsons::IWriter& _writer;

public:
  JsonDiagnosticsCollector(jsons::IWriter& writer) : _writer(writer) {
    _writer.openObject();
  };

  virtual void beginSection(const char* name) override {
    _writer.property(name);
    _writer.openObject();
  }

  virtual void addValue(const char* name, const char* value) {
    _writer.property(name).string(value);
  }

  virtual void endSection() override {
    _writer.close();
  }
};

}

#endif
