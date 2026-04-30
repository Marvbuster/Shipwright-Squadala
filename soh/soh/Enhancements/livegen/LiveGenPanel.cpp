#include "LiveGenPanel.h"
#include "LiveGenEntrance.h"
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
    ImGui::Text("Squadala");
    ImGui::Separator();
    DrawStatusBar();
    ImGui::Separator();
    DrawChatHistory();
    ImGui::Separator();
    DrawInputArea();
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
        case State::COMPLETE: ImGui::TextColored(ImVec4(0.1f, 1, 0.1f, 1), "Done!"); break;
        case State::ERROR:    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error"); break;
    }
}

void LiveGenPanel::DrawChatHistory() {
    ImGui::BeginChild("ChatHistory", ImVec2(0, -80), true);

    if (mChatHistory.empty()) {
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

    if (mState == State::COMPLETE && !mLastSpecJson.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "Dungeon Spec:");
        ImGui::BeginChild("SpecPreview", ImVec2(0, 80), true);
        ImGui::TextWrapped("%s", mLastSpecJson.c_str());
        ImGui::EndChild();

        // ENTER DUNGEON button
        bool portalActive = EntranceManager::Instance().IsActive();
        if (portalActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 1.0f, 1.0f));
            if (ImGui::Button("Portal Active! Walk through any door...", ImVec2(ImGui::GetContentRegionAvail().x, 32))) {
                EntranceManager::Instance().Deactivate();
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            if (ImGui::Button("Enter Dungeon", ImVec2(ImGui::GetContentRegionAvail().x, 32))) {
                // Activate portal — next door goes to Deku Tree entrance (0x0000)
                EntranceManager::Instance().Activate(0x0000);
            }
            ImGui::PopStyleColor(2);
        }
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void LiveGenPanel::DrawInputArea() {
    if (mState == State::IDLE || mState == State::COMPLETE || mState == State::ERROR) {
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
        mChatHistory.push_back({ChatMessage::SYSTEM, "Dungeon '" + name + "' generated!"});
        mState = State::COMPLETE;
        SPDLOG_INFO("LiveGen: Dungeon generated: {}", name);
    } else if (result.status == "error") {
        std::string err = result.error.value_or("Unknown error");
        mChatHistory.push_back({ChatMessage::SYSTEM, "Error: " + err});
        mState = State::ERROR;
    }
}

} // namespace LiveGen
