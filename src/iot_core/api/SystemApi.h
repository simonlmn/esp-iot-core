#ifndef IOT_CORE_API_SYSTEMAPI_H_
#define IOT_CORE_API_SYSTEMAPI_H_

#include <iot_core/Interfaces.h>
#include <iot_core/Config.h>
#include <uri/UriBraces.h>
#include <jsons.h>
#include "Interfaces.h"
#include "JsonDiagnosticsCollector.h"

namespace iot_core::api {

class SystemApi final : public IProvider {
private:
  iot_core::Logger _logger;
  iot_core::ISystem& _system;
  iot_core::IApplicationContainer& _application;

public:
  SystemApi(iot_core::ISystem& system, iot_core::IApplicationContainer& application) : _logger(system.logger("api")), _system(system), _application(application) {}

  void setupApi(IServer& server) override {
    server.on(F("/api/system/reset"), HttpMethod::POST, [this](IRequest&, IResponse& response) {
      _system.schedule([&] () { _system.reset(); });
      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/factory-reset"), HttpMethod::POST, [this](IRequest&, IResponse& response) {
      _system.schedule([&] () { _system.factoryReset(); });
      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/stop"), HttpMethod::POST, [this](IRequest&, IResponse& response) {
      _system.stop();
      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/status"), HttpMethod::GET, [this](IRequest&, IResponse& response) {
      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::ApplicationJson)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      auto writer = jsons::makeWriter(body);
      JsonDiagnosticsCollector collector {writer};
      _application.getDiagnostics(collector);
      writer.end();
      if (writer.failed()) {
        _logger.log(LogLevel::Warning, "Failed to write diagnostics JSON response.");
      }
    });

    server.on(F("/api/system/logs"), HttpMethod::GET, [this](IRequest&, IResponse& response) {
      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }
      
      _system.logs().output([&] (const char* entry) {
        body.write(entry);
      });
    });

    server.on(F("/api/system/components"), HttpMethod::GET, [this](IRequest&, IResponse& response) {
      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::ApplicationJson)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      auto writer = jsons::makeWriter(body);
      writer.openList();

      _application.forEachComponent([&] (const IApplicationComponent* component) {
        writer.openObject();
        writer.property("name").string(component->name());
        
        writer.property("config").openObject();
        component->getConfig([&] (const char* name, const char* value) {
          writer.property(name).string(value);
        });
        writer.close();

        writer.property("logLevel").string(iot_core::logLevelToString(_system.logs().logLevel(component->name())));

        writer.property("diagnostics");
        JsonDiagnosticsCollector collector {writer};
        component->getDiagnostics(collector);
        collector.end();
        
        writer.close();
      });

      writer.close();
      writer.end();
      
      if (writer.failed()) {
        _logger.log(LogLevel::Warning, "Failed to write components JSON response.");
      }
    });

    server.on(UriBraces(F("/api/system/components/{}")), HttpMethod::GET, [this](IRequest& request, IResponse& response) {
      const auto& name = request.pathArg(0);

      const auto component = _application.getComponent(name);
      if (component == nullptr) {
        response.code(ResponseCode::BadRequest)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(F("Component not found"));
        return;
      }

      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::ApplicationJson)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      auto writer = jsons::makeWriter(body);      
      writer.openObject();
      writer.property("name").string(component->name());
      
      writer.property("config").openObject();
      component->getConfig([&] (const char* name, const char* value) {
        writer.property(name).string(value);
      });
      writer.close();

      writer.property("logLevel").string(iot_core::logLevelToString(_system.logs().logLevel(component->name())));

      writer.property("diagnostics");
      JsonDiagnosticsCollector collector {writer};
      component->getDiagnostics(collector);
      collector.end();
      
      writer.close();
      writer.end();
      
      if (writer.failed()) {
        _logger.log(LogLevel::Warning, "Failed to write components JSON response.");
      }
    });

    server.on(UriBraces(F("/api/system/components/{}/log-level")), HttpMethod::GET, [this](IRequest& request, IResponse& response) {
      const auto& name = request.pathArg(0);

      const auto component = _application.getComponent(name);
      if (component == nullptr) {
        response.code(ResponseCode::BadRequest)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(F("Component not found"));
        return;
      }

      response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendSingleBody()
        .write(iot_core::logLevelToString(_system.logs().logLevel(name.cstr())));
    });

    server.on(UriBraces(F("/api/system/components/{}/log-level")), HttpMethod::PUT, [this](IRequest& request, IResponse& response) {
      const auto& name = request.pathArg(0);
      const auto component = _application.getComponent(name);
      if (component == nullptr) {
        response.code(ResponseCode::BadRequest)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(F("Component not found"));
        return;
      }

      iot_core::LogLevel logLevel = iot_core::logLevelFromString(request.body().content());
      if (logLevel == iot_core::LogLevel::Unknown) {
        response.code(ResponseCode::BadRequest)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(F("Invalid log level"));
        return;
      }

      _system.logs().logLevel(iot_core::make_static(name.cstr()), logLevel);
      
      response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendSingleBody()
        .write(iot_core::logLevelToString(_system.logs().logLevel(name.cstr())));
    });

    server.on(UriBraces(F("/api/system/components/{}/log-level")), HttpMethod::DELETE, [this](IRequest& request, IResponse& response) {
      const auto& name = request.pathArg(0);

      const auto component = _application.getComponent(name);
      if (component == nullptr) {
        response.code(ResponseCode::BadRequest)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(F("Component not found"));
        return;
      }

      _system.logs().clearLogLevel(name.cstr());

      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/log-level"), HttpMethod::GET, [this](IRequest&, IResponse& response) {
      response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendSingleBody()
        .write(iot_core::logLevelToString(_system.logs().initialLogLevel()));
    });

    server.on(F("/api/system/log-level"), HttpMethod::PUT, [this](IRequest& request, IResponse& response) {
      iot_core::LogLevel logLevel = iot_core::logLevelFromString(request.body().content());

      _system.logs().initialLogLevel(logLevel);
      
      response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendSingleBody()
        .write(iot_core::logLevelToString(_system.logs().initialLogLevel()));
    });

    server.on(F("/api/system/config"), HttpMethod::GET, [this](IRequest&, IResponse& response) {
      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      _application.getAllConfig([&] (const char* path, const char* value) {
        body.write(path);
        body.write(iot_core::ConfigParser::SEPARATOR);
        body.write(value);
        body.write(iot_core::ConfigParser::END);
        body.write('\n');
      });
    });

    server.on(F("/api/system/config"), HttpMethod::PUT, [this](IRequest& request, IResponse& response) {
      const char* body = request.body().content().cstr();

      iot_core::ConfigParser config {const_cast<char*>(body)};

      if (_application.configureAll(config)) {
        response
          .code(ResponseCode::Ok)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(body);
      } else {
        response.code(ResponseCode::BadRequest);
      }
    });

    server.on(UriBraces(F("/api/system/config/{}")), HttpMethod::GET, [this](IRequest& request, IResponse& response) {
      const auto& category = request.pathArg(0);

      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      _application.getConfig(category.cstr(), [&] (const char* name, const char* value) {
        body.write(name);
        body.write(iot_core::ConfigParser::SEPARATOR);
        body.write(value);
        body.write(iot_core::ConfigParser::END);
        body.write('\n');
      });
    });

    server.on(UriBraces(F("/api/system/config/{}")), HttpMethod::PUT, [this](IRequest& request, IResponse& response) {
      const auto& category = request.pathArg(0);
      const char* body = request.body().content().cstr();

      iot_core::ConfigParser config {const_cast<char*>(body)};

      if (_application.configure(category.cstr(), config)) {
        response
          .code(ResponseCode::Ok)
          .contentType(ContentType::TextPlain)
          .sendSingleBody()
          .write(body);
      } else {
        response.code(ResponseCode::BadRequest);
      }
    });
  }
};

}

#endif
