#pragma once

#include <folly/Conv.h>
#include <folly/Format.h>

namespace nozomi {
// TODO: Make this an anonymous namespace?
namespace route {

enum class RouteParamType {
  Int64,
  Double,
  String,
  OptionalInt64,
  OptionalDouble,
  OptionalString,
};

std::string to_string(RouteParamType param);
std::ostream& operator<<(std::ostream& out, RouteParamType param);

template <typename T>
inline RouteParamType to_RouteParamType();

template <>
inline RouteParamType to_RouteParamType<int64_t>() {
  return RouteParamType::Int64;
}

template <>
inline RouteParamType to_RouteParamType<folly::Optional<int64_t>>() {
  return RouteParamType::OptionalInt64;
}

template <>
inline RouteParamType to_RouteParamType<double>() {
  return RouteParamType::Double;
}

template <>
inline RouteParamType to_RouteParamType<folly::Optional<double>>() {
  return RouteParamType::OptionalDouble;
}

template <>
inline RouteParamType to_RouteParamType<std::string>() {
  return RouteParamType::String;
}

template <>
inline RouteParamType to_RouteParamType<folly::Optional<std::string>>() {
  return RouteParamType::OptionalString;
}

template <typename T>
inline T get_handler_args(int N, const boost::smatch& matches);

template <>
inline int64_t get_handler_args<int64_t>(int N, const boost::smatch& matches) {
  // TODO: There is probably a way to make all of these sformat("__{}", N)
  //        calls constexpr
  try {
    return folly::to<int64_t>(matches[folly::sformat("__{}", N)].str());
  } catch (const std::exception& e) {
    // TODO: Logging
    return std::numeric_limits<int64_t>::max();
  }
}

template <>
inline double get_handler_args<double>(int N, const boost::smatch& matches) {
  return folly::to<double>(matches[folly::sformat("__{}", N)].str());
}

template <>
inline std::string get_handler_args<std::string>(int N,
                                                 const boost::smatch& matches) {
  return matches[folly::sformat("__{}", N)].str();
}

template <>
inline folly::Optional<int64_t> get_handler_args<folly::Optional<int64_t>>(
    int N, const boost::smatch& matches) {
  folly::Optional<int64_t> ret;
  const auto& match = matches[folly::sformat("__{}", N)];
  if (match.matched) {
    try {
      ret = folly::to<int64_t>(match.str());
    } catch (const std::exception& e) {
      // TODO: Logging
      ret = std::numeric_limits<int64_t>::max();
    }
  }
  return ret;
}

template <>
inline folly::Optional<double> get_handler_args<folly::Optional<double>>(
    int N, const boost::smatch& matches) {
  folly::Optional<double> ret;
  const auto& match = matches[folly::sformat("__{}", N)];
  if (match.matched) {
    ret = folly::to<double>(match.str());
  }
  return ret;
}

template <>
inline folly::Optional<std::string>
get_handler_args<folly::Optional<std::string>>(int N,
                                               const boost::smatch& matches) {
  folly::Optional<std::string> ret;
  const auto& match = matches[folly::sformat("__{}", N)];
  if (match.matched) {
    ret = match.str();
  }
  return ret;
}

template <int N, typename T>
inline T get_handler_args(const boost::smatch& matches) {
  return get_handler_args<T>(N, matches);
}

template <typename... HandlerArgs, std::size_t... N>
inline void call_streaming_handler(
    std::index_sequence<N...>,
    type_sequence<HandlerArgs...>,
    StreamingHTTPHandler<HandlerArgs...>& handler,
    const boost::smatch& matches) {
  return handler.setRequestArgs(get_handler_args<N, HandlerArgs>(matches)...);
}

template <typename... HandlerArgs>
inline void call_streaming_handler(
    StreamingHTTPHandler<HandlerArgs...>& handler,
    const boost::smatch& matches) {
  return call_streaming_handler(std::index_sequence_for<HandlerArgs...>{},
                                type_sequence<HandlerArgs...>{}, handler,
                                matches);
}

template <typename HandlerType, typename... HandlerArgs, std::size_t... N>
inline folly::Future<HTTPResponse> call_handler(std::index_sequence<N...>,
                                                type_sequence<HandlerArgs...>,
                                                HandlerType& f,
                                                const HTTPRequest& request,
                                                const boost::smatch& matches) {
  return f(request, get_handler_args<N, HandlerArgs>(matches)...);
}

template <typename HandlerType, typename... HandlerArgs>
inline folly::Future<HTTPResponse> call_handler(HandlerType& f,
                                                const HTTPRequest& request,
                                                const boost::smatch& matches) {
  return call_handler(std::index_sequence_for<HandlerArgs...>{},
                      type_sequence<HandlerArgs...>{}, f, request, matches);
}

template <typename HandlerType, bool IsStreaming, typename... HandlerArgs>
struct RouteMatchMaker {};

template <typename HandlerType, typename... HandlerArgs>
struct RouteMatchMaker<HandlerType, false, HandlerArgs...> {
  inline RouteMatch operator()(std::shared_ptr<const std::string>& path,
                               boost::smatch& matches,
                               HandlerType& handler) {
    return RouteMatch(
        RouteMatchResult::RouteMatched,
        std::function<folly::Future<HTTPResponse>(const HTTPRequest&)>([
          path = std::move(path), matches = std::move(matches), &handler
        ](const HTTPRequest& request) mutable {
          // We have to hold onto the original path variable because
          // the matches object
          // retains a reference to the string that was matched on.
          // We use a unique_ptr
          // because there's no absolute guarantee that an std::move
          // won't invalidate the
          // reference to the original string.
          return route::call_handler<HandlerType, HandlerArgs...>(
              handler, request, matches);
        }));
  }
};

template <typename HandlerType, typename... HandlerArgs>
struct RouteMatchMaker<HandlerType, true, HandlerArgs...> {
  inline RouteMatch operator()(std::shared_ptr<const std::string>& path,
                               boost::smatch& matches,
                               HandlerType& handler) {
    // TODO: Fill this out properly
    return RouteMatch(
        RouteMatchResult::RouteMatched,
        std::function<folly::Future<HTTPResponse>(const HTTPRequest&)>(),
        std::function<proxygen::RequestHandler*()>([
          path = std::move(path), matches = std::move(matches), &handler
        ]() {
          // TODO: Exception handling to cleanup memory
          auto ret = handler();
          route::call_streaming_handler<HandlerArgs...>(*ret, matches);
          return ret;
        }));

    return RouteMatch(RouteMatchResult::PathNotMatched);
  }
};

template <typename... Args>
inline typename std::enable_if<sizeof...(Args) == 0, void>::type
parse_function_parameters(std::vector<RouteParamType>& params) {}

template <typename Arg1, typename... Args>
inline void parse_function_parameters(std::vector<RouteParamType>& params) {
  params.push_back(to_RouteParamType<Arg1>());
  parse_function_parameters<Args...>(params);
}

template <typename... Args>
std::vector<RouteParamType> parse_function_parameters() {
  std::vector<RouteParamType> params;
  params.reserve(sizeof...(Args));
  parse_function_parameters<Args...>(params);
  return params;
}

std::pair<boost::basic_regex<char>, std::vector<RouteParamType>>
parse_route_pattern(const std::string& route);

}  // end namespace route

template <typename HandlerType, typename... HandlerArgs>
RouteMatch Route<HandlerType, HandlerArgs...>::handler(
    const proxygen::HTTPMessage* request) {
  DCHECK(request != nullptr) << "Request must not be null";
  boost::smatch matches;
  auto methodAndPath = HTTPRequest::getMethodAndPath(request);
  // TODO: Make this uniuqe when using folly::function
  auto path = std::make_shared<const std::string>(
      std::move(std::get<1>(methodAndPath)));
  if (!boost::regex_match(*path, matches, regex_)) {
    return RouteMatch(RouteMatchResult::PathNotMatched);
  }
  if (methods_.find(std::get<0>(methodAndPath)) == methods_.end()) {
    return RouteMatch(RouteMatchResult::MethodNotMatched);
  }

  return route::RouteMatchMaker<
      HandlerType,
      std::is_convertible<decltype(std::declval<HandlerType>()(std::declval<const HTTPRequest&>())),
                          StreamingHTTPHandler<HandlerArgs...>*>::value,
      HandlerArgs...>{}(path, matches, handler_);
}

template <typename HandlerType, typename... HandlerArgs>
Route<HandlerType, HandlerArgs...>::Route(
    std::string pattern,
    std::unordered_set<proxygen::HTTPMethod> methods,
    HandlerType handler)
    : BaseRoute(std::move(pattern), std::move(methods), false),
      handler_(std::move(handler)) {
  auto regexAndPatternParams = route::parse_route_pattern(originalPattern_);
  auto functionParams = route::parse_function_parameters<HandlerArgs...>();
  const auto& patternParams = regexAndPatternParams.second;
  regex_ = std::move(regexAndPatternParams.first);

  if (patternParams.size() != functionParams.size()) {
    auto error = folly::sformat(
        "Pattern parameter count != function parameter count ({} vs {})\n",
        patternParams.size(), functionParams.size());
    folly::format(&error, "Pattern parameters:\n");
    for (const auto& param : patternParams) {
      folly::format(&error, "{}\n", to_string(param));
    }
    folly::format(&error, "Function parameters:\n");
    for (const auto& param : functionParams) {
      folly::format(&error, "{}\n", to_string(param));
    }
    throw std::runtime_error(error);
  }

  for (size_t i = 0; i < patternParams.size(); ++i) {
    if (patternParams[i] != functionParams[i]) {
      throw std::runtime_error(folly::sformat(
          "Pattern parameter {} ({}) does not match function parameter {} ({})",
          i, to_string(patternParams[i]), i, to_string(functionParams[i])));
    }
  }
}
}
