#include "LiveGenClient.h"
#include <spdlog/spdlog.h>
#include <curl/curl.h>

using json = nlohmann::json;

namespace LiveGen {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

Client::Client(const std::string& baseUrl) : mBaseUrl(baseUrl) {}

static GenerationResult ParseResult(const json& r) {
    GenerationResult result;
    result.status = r.value("status", "error");

    if (r.contains("question") && !r["question"].is_null()) {
        PlayerQuestion q;
        q.question = r["question"].value("question", "");
        if (r["question"].contains("options") && !r["question"]["options"].is_null()) {
            for (auto& opt : r["question"]["options"]) {
                q.options.push_back(opt.get<std::string>());
            }
        }
        result.question = q;
    }
    if (r.contains("spec") && !r["spec"].is_null()) {
        result.spec = r["spec"];
    }
    if (r.contains("error") && !r["error"].is_null()) {
        result.error = r["error"].get<std::string>();
    }
    return result;
}

SessionResponse Client::CreateSession(const std::string& prompt) {
    json body = {{"prompt", prompt}};
    std::string response = HttpPost(mBaseUrl + "/sessions", body);

    SessionResponse sr;
    try {
        json j = json::parse(response);
        sr.sessionId = j.value("session_id", "");
        sr.result = ParseResult(j["result"]);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("LiveGen: Failed to parse session response: {}", e.what());
        sr.result.status = "error";
        sr.result.error = std::string("Parse error: ") + e.what();
    }
    return sr;
}

GenerationResult Client::SendMessage(const std::string& sessionId, const std::string& message) {
    json body = {{"message", message}};
    std::string response = HttpPost(mBaseUrl + "/sessions/" + sessionId + "/message", body);

    try {
        json j = json::parse(response);
        return ParseResult(j["result"]);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("LiveGen: Failed to parse message response: {}", e.what());
        return {"error", std::nullopt, std::nullopt, std::string("Parse error: ") + e.what()};
    }
}

bool Client::IsHealthy() {
    try {
        std::string response = HttpGet(mBaseUrl + "/health");
        json j = json::parse(response);
        return j.value("status", "") == "ok";
    } catch (...) {
        return false;
    }
}

std::string Client::HttpPost(const std::string& url, const json& body) {
    std::string responseStr;
    CURL* curl = curl_easy_init();
    if (!curl) return R"({"error":"curl init failed"})";

    std::string bodyStr = body.dump();
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        SPDLOG_ERROR("LiveGen HTTP POST failed: {}", curl_easy_strerror(res));
        responseStr = R"({"error":")" + std::string(curl_easy_strerror(res)) + R"("})";
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return responseStr;
}

std::string Client::HttpGet(const std::string& url) {
    std::string responseStr;
    CURL* curl = curl_easy_init();
    if (!curl) return R"({"error":"curl init failed"})";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseStr);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        responseStr = R"({"error":")" + std::string(curl_easy_strerror(res)) + R"("})";
    }

    curl_easy_cleanup(curl);
    return responseStr;
}

} // namespace LiveGen
