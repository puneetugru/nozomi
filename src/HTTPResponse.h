#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <folly/futures/Future.h>
#include <folly/io/IOBuf.h>
#include <folly/json.h>
#include <proxygen/lib/http/HTTPHeaders.h>
#include <proxygen/lib/http/HTTPMessage.h>

#include "src/EnumHash.h"
#include "src/StringUtils.h"

namespace nozomi {

/**
 * Represents an HTTP response and body
 */
class HTTPResponse {
 private:
  proxygen::HTTPMessage response_;
  std::unique_ptr<folly::IOBuf> body_;

 public:
  HTTPResponse();
  HTTPResponse(int16_t statusCode,
               const folly::dynamic& body,
               std::unordered_map<std::string, std::string> headers);
  HTTPResponse(int16_t statusCode,
               const std::string& body,
               std::unordered_map<std::string, std::string> headers);
  HTTPResponse(int16_t statusCode,
               std::unique_ptr<folly::IOBuf> body,
               std::unordered_map<std::string, std::string> headers);
  HTTPResponse(
      int16_t statusCode,
      const folly::dynamic& body,
      std::unordered_map<proxygen::HTTPHeaderCode, std::string> headers);
  HTTPResponse(
      int16_t statusCode,
      const std::string& body,
      std::unordered_map<proxygen::HTTPHeaderCode, std::string> headers);
  HTTPResponse(
      int16_t statusCode,
      std::unique_ptr<folly::IOBuf> body,
      std::unordered_map<proxygen::HTTPHeaderCode, std::string> headers);

  HTTPResponse(int16_t statusCode);
  HTTPResponse(int16_t statusCode, const std::string& body);

  // string converts to dynamic, and I can't seem to disable conversion
  // for specific arguments. Different names should do it, though
  static inline folly::Future<HTTPResponse> fromJson(
      int16_t statusCode,
      const folly::dynamic& body,
      std::unordered_map<std::string, std::string> headers) {
    return future(statusCode, body, std::move(headers));
  }
  static inline folly::Future<HTTPResponse> fromString(
      int16_t statusCode,
      const std::string& body,
      std::unordered_map<std::string, std::string> headers) {
    return future(statusCode, body, std::move(headers));
  }
  static inline folly::Future<HTTPResponse> fromBytes(
      int16_t statusCode,
      std::unique_ptr<folly::IOBuf> body,
      std::unordered_map<std::string, std::string> headers) {
    return future(statusCode, std::move(body), std::move(headers));
  }
  static inline folly::Future<HTTPResponse> fromJson(
      int16_t statusCode,
      const folly::dynamic& body,
      std::unordered_map<proxygen::HTTPHeaderCode, std::string> headers) {
    return future(statusCode, body, std::move(headers));
  }
  static inline folly::Future<HTTPResponse> fromString(
      int16_t statusCode,
      const std::string& body,
      std::unordered_map<proxygen::HTTPHeaderCode, std::string> headers) {
    return future(statusCode, body, std::move(headers));
  }
  static inline folly::Future<HTTPResponse> fromBytes(
      int16_t statusCode,
      std::unique_ptr<folly::IOBuf> body,
      std::unordered_map<proxygen::HTTPHeaderCode, std::string> headers) {
    return future(statusCode, std::move(body), std::move(headers));
  }

  template <typename... Args>
  static inline folly::Future<HTTPResponse> future(Args&&... args) {
    return folly::makeFuture(HTTPResponse(std::forward<Args>(args)...));
  }

  inline const proxygen::HTTPMessage& getHeaders() const { return response_; }
  inline int16_t getStatusCode() const { return response_.getStatusCode(); }
  inline std::string getBodyString() const { return to_string(body_); }
  inline std::unique_ptr<folly::IOBuf> getBody() const {
    return body_->clone();
  }
};
}
