#pragma once

#include <ship/window/gui/GuiWindow.h>
#include "LiveGenClient.h"
#include <thread>
#include <mutex>
#include <atomic>

namespace LiveGen {

class LiveGenPanel : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override;

  private:
    Client mClient;
    std::string mSessionId;
    std::string mPromptInput;
    std::string mReplyInput;

    struct ChatMessage {
        enum Type { PLAYER, AGENT, SYSTEM };
        Type type;
        std::string text;
    };
    std::vector<ChatMessage> mChatHistory;

    enum class State { IDLE, WAITING, QUESTION, COMPLETE, ERROR };
    State mState = State::IDLE;
    std::string mLastSpecJson;

    // Background thread for HTTP calls
    std::mutex mResultMutex;
    std::optional<GenerationResult> mPendingResult;
    std::optional<std::string> mPendingSessionId;
    std::atomic<bool> mRequestInFlight{false};

    void StartSession();
    void SendReply();
    void HandleResult(const GenerationResult& result);
    void CheckPendingResult();
    void DrawChatHistory();
    void DrawInputArea();
    void DrawStatusBar();
};

} // namespace LiveGen
