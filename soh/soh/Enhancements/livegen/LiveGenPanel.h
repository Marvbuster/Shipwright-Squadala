#pragma once

#include <ship/window/gui/GuiWindow.h>
#include "LiveGenClient.h"

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

    void StartSession();
    void SendReply();
    void HandleResult(const GenerationResult& result);
    void DrawChatHistory();
    void DrawInputArea();
    void DrawStatusBar();
};

} // namespace LiveGen
