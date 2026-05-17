#pragma once

#include "utils.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <glm/glm.hpp>
#include <mutex>

namespace kronk
{
enum class MessageType : u32
{
  ClearScreen,
  SubmitUnit,
};

struct ClearScreen
{
  MessageType type{MessageType::ClearScreen};
  glm::vec4   color{};
};

struct SubmitUnit
{
  MessageType type{MessageType::SubmitUnit};
  glm::vec2   rectSize{};
  glm::vec2   position{};
};

union Message
{
  MessageType type;
  ClearScreen clearScreen;
  SubmitUnit  submitUnit;
};

class MessageQueue final
{
  std::deque<Message> mMessages{};
  // std::mutex              mQueueMutex{};
  // std::condition_variable mHasMessages{};

public:
  void    SendMessage(const Message &message);
  Message GetMessage();
  void    BlockUntilMessages();
};
} // namespace kronk