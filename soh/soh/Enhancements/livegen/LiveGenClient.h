#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace LiveGen {

struct PlayerQuestion {
    std::string question;
    std::vector<std::string> options;
};

struct GenerationResult {
    std::string status; // "questions", "complete", "error"
    std::optional<PlayerQuestion> question;
    std::optional<nlohmann::json> spec;
    std::optional<std::string> error;
};

struct SessionResponse {
    std::string sessionId;
    GenerationResult result;
};

class Client {
  public:
    Client(const std::string& baseUrl = "http://127.0.0.1:7777");

    SessionResponse CreateSession(const std::string& prompt);
    GenerationResult SendMessage(const std::string& sessionId, const std::string& message);
    bool IsHealthy();

  private:
    std::string mBaseUrl;
    std::string HttpPost(const std::string& url, const nlohmann::json& body);
    std::string HttpGet(const std::string& url);
};

} // namespace LiveGen
