#ifndef IOT_CORE_API_INTERFACES_H_
#define IOT_CORE_API_INTERFACES_H_

#include <Uri.h> /* from ESP8266WebServer library */
#include <functional>
#include <toolbox.h>
#include <toolbox/Streams.h>

namespace iot_core::api {

enum struct HttpMethod {
  ANY, GET, HEAD, POST, PUT, PATCH, DELETE, OPTIONS
};

enum struct ContentType {
  Unknown,
  TextPlain,
  TextCsv,
  TextHtml,
  ApplicationOctetStream,
  ApplicationJson,
  ApplicationXml
};

enum struct ResponseCode : int {
  Ok = 200,
  OkCreated = 201,
  OkAccepted = 202,
  OkNoContent = 204,
  OkPartialContent = 206,
  RedirectMultipleChoices = 300,
  RedirectMovedPermanently = 301,
  RedirectFound = 302,
  RedirectSeeOther = 303,
  RedirectNotModified = 304,
  RedirectTemporary = 307,
  RedirectPermanent = 308,
  BadRequest = 400,
  BadRequestUnauthorized = 401,
  BadRequestForbidden = 403,
  BadRequestNotFound = 404,
  BadRequestMethodNotAllowed = 405,
  BadRequestNotAcceptable = 406,
  BadRequestTimeout = 408,
  BadRequestConflict = 409,
  BadRequestGone = 410,
  BadRequestLengthRequired = 411,
  BadRequestPreconditionFailed = 412,
  BadRequestTooManyRequests = 429,
  InternalServerError = 500,
  NotImplemented = 501,
  BadGateway = 502,
  ServiceUnavailable = 503,
  GatewayTimeout = 504,
  HttpVersionNotSupported = 505,
  InsufficientStorage = 507
};

class IRequestBody : public toolbox::IInput {
public:
  virtual const toolbox::strref& contentType() const = 0;
  virtual const toolbox::strref& content() const = 0;
};

class IRequest {
public:
  virtual bool hasArg(const toolbox::strref& name) const = 0;
  virtual toolbox::strref arg(const toolbox::strref& name) const = 0;
  virtual toolbox::strref pathArg(unsigned int i) const = 0;
  virtual const IRequestBody& body() const = 0;
};

class IResponseBody : public toolbox::IOutput {
public:
  virtual bool valid() const = 0;
  operator bool() const { return valid(); }
};

class IResponse {
public:
  virtual IResponse& code(ResponseCode code) = 0;
  virtual IResponse& contentType(ContentType contentType) = 0;
  virtual IResponse& contentType(const toolbox::strref& contentType) = 0; 
  virtual IResponse& header(const toolbox::strref& name, const toolbox::strref& value) = 0;
  virtual IResponseBody& sendChunkedBody() = 0;
  virtual IResponseBody& sendSingleBody() = 0;
};

class IServer {
public:
  virtual void on(const Uri& uri, HttpMethod method, std::function<void(const IRequest&, IResponse&)> handler) = 0;
};

class IProvider {
public:
  virtual void setupApi(IServer& server) = 0;
};

class IContainer {
public:
  virtual void addProvider(IProvider* provider) = 0;
};

}

#endif
