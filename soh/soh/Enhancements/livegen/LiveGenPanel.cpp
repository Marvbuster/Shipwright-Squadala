#include "LiveGenPanel.h"
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace LiveGen {

void LiveGenPanel::InitElement() {
    mPromptInput.resize(512, '\0');
    mReplyInput.resize(256, '\0');
}

void LiveGenPanel::UpdateElement() {}

void LiveGenPanel::DrawElement() {
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    ImGui::Text("Dungeon Generator");
    ImGui::Separator();
    DrawStatusBar();
    ImGui::Separator();
    DrawChatHistory();
    ImGui::Separator();
    DrawInputArea();
}

void LiveGenPanel::DrawStatusBar() {
    // Check sidecar health (cached, not every frame)
    static bool healthy = false;
    static int frameCounter = 0;
    if (frameCounter++ % 300 == 0) { // Check every ~5 seconds at 60fps
        healthy = mClient.IsHealthy();
    }

    if (healthy) {
        ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "Sidecar: Connected");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Sidecar: Not Running");
        ImGui::SameLine();
        ImGui::TextDisabled("(Start: uv run uvicorn livegen.api:app --port 7777)");
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 150);
    switch (mState) {
        case State::IDLE:     ImGui::TextDisabled("Ready"); break;
        case State::WAITING:  ImGui::TextColored(ImVec4(1, 1, 0, 1), "Thinking..."); break;
        case State::QUESTION: ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "Your turn"); break;
        case State::COMPLETE: ImGui::TextColored(ImVec4(0.1f, 1, 0.1f, 1), "Dungeon Ready!"); break;
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
        ImGui::TextDisabled("  'Something easy for a beginner, forest theme'");
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

    if (mState == State::COMPLETE && !mLastSpecJson.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.1f, 1.0f, 0.1f, 1.0f), "Generated Dungeon Spec:");
        ImGui::BeginChild("SpecPreview", ImVec2(0, 150), true);
        ImGui::TextWrapped("%s", mLastSpecJson.c_str());
        ImGui::EndChild();
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void LiveGenPanel::DrawInputArea() {
    if (mState == State::IDLE || mState == State::COMPLETE || mState == State::ERROR) {
        ImGui::Text("Prompt:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-80);
        if (ImGui::InputText("##prompt", mPromptInput.data(), mPromptInput.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            StartSession();
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate", ImVec2(70, 0))) {
            StartSession();
        }
    } else if (mState == State::QUESTION) {
        ImGui::Text("Reply:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-80);
        if (ImGui::InputText("##reply", mReplyInput.data(), mReplyInput.size(),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            SendReply();
        }
        ImGui::SameLine();
        if (ImGui::Button("Send", ImVec2(70, 0))) {
            SendReply();
        }
    } else if (mState == State::WAITING) {
        ImGui::TextDisabled("The dungeon architect is thinking...");
    }
}

void LiveGenPanel::StartSession() {
    std::string prompt(mPromptInput.c_str());
    if (prompt.empty()) return;

    mChatHistory.clear();
    mChatHistory.push_back({ChatMessage::PLAYER, prompt});
    mState = State::WAITING;

    // TODO: Background thread to avoid blocking game
    auto response = mClient.CreateSession(prompt);
    mSessionId = response.sessionId;
    HandleResult(response.result);

    std::fill(mPromptInput.begin(), mPromptInput.end(), '\0');
}

void LiveGenPanel::SendReply() {
    std::string reply(mReplyInput.c_str());
    if (reply.empty() || mSessionId.empty()) return;

    mChatHistory.push_back({ChatMessage::PLAYER, reply});
    mState = State::WAITING;

    auto result = mClient.SendMessage(mSessionId, reply);
    HandleResult(result);

    std::fill(mReplyInput.begin(), mReplyInput.end(), '\0');
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
