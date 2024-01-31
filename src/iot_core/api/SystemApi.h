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
  iot_core::ISystem& _system;
  iot_core::IApplicationContainer& _application;

public:
  SystemApi(iot_core::ISystem& system, iot_core::IApplicationContainer& application) : _system(system), _application(application) {}

  void setupApi(IServer& server) override {
    server.on(F("/api/system/reset"), HttpMethod::POST, [this](const IRequest&, IResponse& response) {
      _system.schedule([&] () { _system.reset(); });
      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/factory-reset"), HttpMethod::POST, [this](const IRequest&, IResponse& response) {
      _system.schedule([&] () { _system.factoryReset(); });
      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/stop"), HttpMethod::POST, [this](const IRequest&, IResponse& response) {
      _system.stop();
      response.code(ResponseCode::OkNoContent);
    });

    server.on(F("/api/system/status"), HttpMethod::GET, [this](const IRequest&, IResponse& response) {
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
        _system.logger().log(LogLevel::Warning, "api", "Failed to write diagnostics JSON response.");
      }
    });

    server.on(F("/api/system/logs"), HttpMethod::GET, [this](const IRequest&, IResponse& response) {
      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }
      
      _system.logger().output([&] (const char* entry) {
        body.write(entry);
      });
    });

    server.on(F("/api/system/log-level"), HttpMethod::GET, [this](const IRequest&, IResponse& response) {
      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      body.write(iot_core::logLevelToString(_system.logger().initialLogLevel()));
      body.write('\n');

      for (auto entry : _system.logger().logLevels()) {
        body.write(entry.first);
        body.write('=');
        body.write(iot_core::logLevelToString(entry.second));
        body.write('\n');
      }
    });

    server.on(F("/api/system/log-level"), HttpMethod::PUT, [this](const IRequest& request, IResponse& response) {
      iot_core::LogLevel logLevel = iot_core::logLevelFromString(request.body().content());

      _system.logger().initialLogLevel(logLevel);
      
      response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendSingleBody()
        .write(iot_core::logLevelToString(_system.logger().initialLogLevel()));
    });

    server.on(UriBraces(F("/api/system/log-level/{}")), HttpMethod::PUT, [this](const IRequest& request, IResponse& response) {
      const auto& category = request.pathArg(0);
      iot_core::LogLevel logLevel = iot_core::logLevelFromString(request.body().content());

      _system.logger().logLevel(iot_core::make_static(category), logLevel);
      
      response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendSingleBody()
        .write(iot_core::logLevelToString(_system.logger().logLevel(category)));
    });

    server.on(F("/api/system/config"), HttpMethod::GET, [this](const IRequest&, IResponse& response) {
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

    server.on(F("/api/system/config"), HttpMethod::PUT, [this](const IRequest& request, IResponse& response) {
      const char* body = request.body().content();

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

    server.on(UriBraces(F("/api/system/config/{}")), HttpMethod::GET, [this](const IRequest& request, IResponse& response) {
      const auto& category = request.pathArg(0);

      IResponseBody& body = response
        .code(ResponseCode::Ok)
        .contentType(ContentType::TextPlain)
        .sendChunkedBody();
      
      if (!body.valid()) {
        return;
      }

      _application.getConfig(category, [&] (const char* name, const char* value) {
        body.write(name);
        body.write(iot_core::ConfigParser::SEPARATOR);
        body.write(value);
        body.write(iot_core::ConfigParser::END);
        body.write('\n');
      });
    });

    server.on(UriBraces(F("/api/system/config/{}")), HttpMethod::PUT, [this](const IRequest& request, IResponse& response) {
      const auto& category = request.pathArg(0);
      const char* body = request.body().content();

      iot_core::ConfigParser config {const_cast<char*>(body)};

      if (_application.configure(category, config)) {
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
