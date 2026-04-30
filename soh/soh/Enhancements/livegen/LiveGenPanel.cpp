#include "LiveGenPanel.h"
#include "LiveGenEntrance.h"
#include "LiveGenHotReload.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace LiveGen {

void LiveGenPanel::InitElement() {
    mPromptInput.resize(512, '\0');
    mReplyInput.resize(256, '\0');
}

void LiveGenPanel::UpdateElement() {
    CheckPendingResult();
}

void LiveGenPanel::CheckPendingResult() {
    std::lock_guard<std::mutex> lock(mResultMutex);
    if (mPendingSessionId.has_value()) {
        mSessionId = mPendingSessionId.value();
        mPendingSessionId.reset();
    }
    if (mPendingResult.has_value()) {
        HandleResult(mPendingResult.value());
        mPendingResult.reset();
        mRequestInFlight = false;
    }
}

void LiveGenPanel::DrawElement() {
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    DrawStatusBar();
    ImGui::Separator();

    // Tabs
    if (ImGui::BeginTabBar("SquadalaTabs")) {
        if (ImGui::BeginTabItem("Generate")) {
            mActiveTab = Tab::GENERATE;
            DrawChatHistory();
            ImGui::Separator();
            DrawInputArea();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("My Dungeons")) {
            mActiveTab = Tab::DUNGEONS;
            DrawDungeonList();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void LiveGenPanel::DrawStatusBar() {
    static bool healthy = false;
    static int frameCounter = 0;
    if (frameCounter++ % 300 == 0) {
        healthy = mClient.IsHealthy();
    }

    if (healthy) {
        ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "Sidecar: Connected");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Sidecar: Not Running");
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    switch (mState) {
        case State::IDLE:     ImGui::TextDisabled("Ready"); break;
        case State::WAITING:  ImGui::TextColored(ImVec4(1, 1, 0, 1), "Thinking..."); break;
        case State::QUESTION: ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "Your turn"); break;
        case State::COMPLETE: ImGui::TextColored(ImVec4(0.1f, 1, 0.1f, 1), "Ready!"); break;
        case State::ERROR:    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error"); break;
    }
}

void LiveGenPanel::DrawChatHistory() {
    // Use all available height minus input area + padding
    float inputHeight = (mState == State::COMPLETE) ? 52 : 68;
    ImGui::BeginChild("ChatHistory", ImVec2(0, -inputHeight), true);

    if (mChatHistory.empty() && mState == State::IDLE) {
        ImGui::TextDisabled("Describe the dungeon you want to explore...");
        ImGui::TextDisabled("");
        ImGui::TextDisabled("Examples:");
        ImGui::TextDisabled("  'An ice dungeon with 3 rooms and a Mini-Boss'");
        ImGui::TextDisabled("  'A dark shadow temple with key puzzles'");
    }

    for (const auto& msg : mChatHistory) {
        switch (msg.type) {
            case ChatMessage::PLAYER:
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "You:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", msg.text.c_str());
                break;
            case ChatMessage::AGENT:
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Architect:");
                ImGui::SameLine();
                ImGui::TextWrapped("%s", msg.text.c_str());
                break;
            case ChatMessage::SYSTEM:
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", msg.text.c_str());
                break;
        }
        ImGui::Spacing();
    }

    if (mState == State::WAITING) {
        ImGui::TextColored(ImVec4(1, 1, 0, 0.7f), "...");
    }

    // Show dungeon summary nicely (not raw JSON)
    if (mState == State::COMPLETE && !mLastSpecJson.empty()) {
        ImGui::Separator();
        try {
            auto spec = nlohmann::json::parse(mLastSpecJson);
            std::string name = spec.value("metadata", nlohmann::json::object()).value("name", "Dungeon");
            std::string theme = spec.value("metadata", nlohmann::json::object()).value("theme", "?");
            std::string diff = spec.value("metadata", nlohmann::json::object()).value("difficulty", "?");
            int rooms = spec.value("rooms", nlohmann::json::array()).size();
            int conns = spec.value("connections", nlohmann::json::array()).size();

            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", name.c_str());
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s | %s | %d rooms | %d doors",
                theme.c_str(), diff.c_str(), rooms, conns);

            // List rooms
            for (auto& room : spec["rooms"]) {
                std::string rid = room.value("id", "?");
                std::string tmpl = room.value("template", "?");
                int enemies = room.value("actors", nlohmann::json::array()).size();
                int chests = room.value("chests", nlohmann::json::array()).size();
                ImGui::BulletText("%s (%s)%s%s", rid.c_str(), tmpl.c_str(),
                    enemies > 0 ? (" | " + std::to_string(enemies) + " enemies").c_str() : "",
                    chests > 0 ? (" | " + std::to_string(chests) + " chests").c_str() : "");
            }
        } catch (...) {
            ImGui::TextWrapped("%s", mLastSpecJson.c_str());
        }
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void LiveGenPanel::DrawInputArea() {
    if (mState == State::COMPLETE) {
        // ENTER DUNGEON button (replaces Go!)
        bool portalActive = EntranceManager::Instance().IsActive();
        if (portalActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 1.0f, 1.0f));
            if (ImGui::Button("Portal Active! Walk through any door...", ImVec2(ImGui::GetContentRegionAvail().x, 36))) {
                EntranceManager::Instance().Deactivate();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.7f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.9f, 0.3f, 1.0f));
            if (ImGui::Button("Enter Dungeon", ImVec2(ImGui::GetContentRegionAvail().x * 0.7f, 36))) {
                // Hot-reload dungeon and activate portal
                HotReloadDungeon("./mods/zzz_squadala_dungeon.o2r");
                // Get dungeon name from spec
                std::string dname = "Custom Dungeon";
                try {
                    auto spec = nlohmann::json::parse(mLastSpecJson);
                    dname = spec.value("metadata", nlohmann::json::object()).value("name", dname);
                } catch (...) {}
                EntranceManager::Instance().Activate(0x0000, dname);
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            if (ImGui::Button("New", ImVec2(ImGui::GetContentRegionAvail().x, 36))) {
                mState = State::IDLE;
                mChatHistory.clear();
                mLastSpecJson.clear();
                mSessionId.clear();
            }
        }
    } else if (mState == State::IDLE || mState == State::ERROR) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 5);
        if (ImGui::InputTextWithHint("##prompt", "Describe your dungeon...", mPromptInput.data(), mPromptInput.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            StartSession();
        }
        if (ImGui::Button("Go!", ImVec2(ImGui::GetContentRegionAvail().x, 28))) {
            StartSession();
        }
    } else if (mState == State::QUESTION) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 5);
        if (ImGui::InputTextWithHint("##reply", "Your answer...", mReplyInput.data(), mReplyInput.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            SendReply();
        }
        if (ImGui::Button("Send", ImVec2(ImGui::GetContentRegionAvail().x, 28))) {
            SendReply();
        }
    } else if (mState == State::WAITING) {
        ImGui::BeginDisabled();
        ImGui::Button("Thinking...", ImVec2(ImGui::GetContentRegionAvail().x, 28));
        ImGui::EndDisabled();
    }
}

void LiveGenPanel::StartSession() {
    std::string prompt(mPromptInput.c_str());
    if (prompt.empty() || mRequestInFlight) return;

    mChatHistory.clear();
    mChatHistory.push_back({ChatMessage::PLAYER, prompt});
    mState = State::WAITING;
    mRequestInFlight = true;
    std::fill(mPromptInput.begin(), mPromptInput.end(), '\0');

    std::string promptCopy = prompt;
    std::thread([this, promptCopy]() {
        auto response = mClient.CreateSession(promptCopy);
        std::lock_guard<std::mutex> lock(mResultMutex);
        mPendingSessionId = response.sessionId;
        mPendingResult = response.result;
    }).detach();
}

void LiveGenPanel::SendReply() {
    std::string reply(mReplyInput.c_str());
    if (reply.empty() || mSessionId.empty() || mRequestInFlight) return;

    mChatHistory.push_back({ChatMessage::PLAYER, reply});
    mState = State::WAITING;
    mRequestInFlight = true;
    std::fill(mReplyInput.begin(), mReplyInput.end(), '\0');

    std::string replyCopy = reply;
    std::string sessionCopy = mSessionId;
    std::thread([this, sessionCopy, replyCopy]() {
        auto result = mClient.SendMessage(sessionCopy, replyCopy);
        std::lock_guard<std::mutex> lock(mResultMutex);
        mPendingResult = result;
    }).detach();
}

void LiveGenPanel::HandleResult(const GenerationResult& result) {
    if (result.status == "questions" && result.question.has_value()) {
        std::string qText = result.question->question;
        if (!result.question->options.empty()) {
            qText += "\n  Options: ";
            for (size_t i = 0; i < result.question->options.size(); i++) {
                if (i > 0) qText += ", ";
                qText += result.question->options[i];
            }
        }
        mChatHistory.push_back({ChatMessage::AGENT, qText});
        mState = State::QUESTION;
    } else if (result.status == "complete" && result.spec.has_value()) {
        mLastSpecJson = result.spec->dump(2);
        std::string name = result.spec->value("metadata", nlohmann::json::object())
                               .value("name", "Unknown Dungeon");
        mChatHistory.push_back({ChatMessage::SYSTEM, "Dungeon ready!"});
        mState = State::COMPLETE;
        SPDLOG_INFO("LiveGen: Dungeon generated: {}", name);
    } else if (result.status == "error") {
        std::string err = result.error.value_or("Unknown error");
        mChatHistory.push_back({ChatMessage::SYSTEM, "Error: " + err});
        mState = State::ERROR;
    }
}

void LiveGenPanel::RefreshDungeonList() {
    try {
        std::string response = mClient.HttpGet("http://127.0.0.1:7777/dungeons");
        auto data = nlohmann::json::parse(response);
        mActiveDungeonId = data.value("active_id", "");
        mDungeonList.clear();
        for (auto& d : data["dungeons"]) {
            DungeonEntry entry;
            entry.id = d.value("id", "");
            entry.name = d.value("name", "?");
            entry.theme = d.value("theme", "?");
            entry.difficulty = d.value("difficulty", "?");
            entry.rooms = d.value("rooms", 0);
            entry.compiled = d.value("compiled", false);
            mDungeonList.push_back(entry);
        }
    } catch (...) {}
}

void LiveGenPanel::DrawDungeonList() {
    // Refresh every 5 seconds
    if (mDungeonListRefreshCounter++ % 300 == 0) {
        RefreshDungeonList();
    }

    if (mDungeonList.empty()) {
        ImGui::TextDisabled("No dungeons yet. Go to Generate tab!");
        return;
    }

    ImGui::BeginChild("DungeonList", ImVec2(0, -5), true);

    for (auto& dungeon : mDungeonList) {
        bool isActive = (dungeon.id == mActiveDungeonId);

        ImGui::PushID(dungeon.id.c_str());

        // Dungeon name + status
        if (isActive) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", dungeon.name.c_str());
        } else {
            ImGui::Text("%s", dungeon.name.c_str());
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s | %s | %dr",
            dungeon.theme.c_str(), dungeon.difficulty.c_str(), dungeon.rooms);

        // Buttons
        if (isActive && dungeon.compiled) {
            bool portalActive = EntranceManager::Instance().IsActive();
            if (portalActive) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 1.0f, 1.0f));
                if (ImGui::Button("Portal Active! Walk through any door...", ImVec2(ImGui::GetContentRegionAvail().x - 40, 28))) {
                    EntranceManager::Instance().Deactivate();
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.9f, 0.3f, 1.0f));
                if (ImGui::Button("Enter Dungeon", ImVec2(ImGui::GetContentRegionAvail().x - 40, 28))) {
                    HotReloadDungeon("./mods/zzz_squadala_dungeon.o2r");
                    EntranceManager::Instance().Activate(0x0000, dungeon.name);
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X", ImVec2(28, 28))) {
                // TODO: delete
            }
            ImGui::PopStyleColor();
        } else if (isActive && !dungeon.compiled) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Restart SoH to load this dungeon!");
        } else {
            if (ImGui::Button("Activate & Compile", ImVec2(140, 24))) {
                std::string did = dungeon.id;
                std::thread([this, did]() {
                    auto resp = mClient.HttpPost("http://127.0.0.1:7777/dungeons/" + did + "/activate",
                        nlohmann::json::object());
                    SPDLOG_INFO("LiveGen: Activate: {}", resp.substr(0, 200));
                    // Hot-reload so no restart needed!
                    HotReloadDungeon("./mods/zzz_squadala_dungeon.o2r");
                    mDungeonListRefreshCounter = 0;
                }).detach();
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X", ImVec2(28, 24))) {
                // TODO: delete
            }
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

} // namespace LiveGen
