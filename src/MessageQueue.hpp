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
};

struct ClearScreen
{
  MessageType type{MessageType::ClearScreen};
  glm::vec4   color{};
};

union Message
{
  MessageType type;
  ClearScreen clearScreen;
};

class MessageQueue final
{
  std::deque<Message>     mMessages{};
  std::mutex              mQueueMutex{};
  std::condition_variable mHasMessages{};

public:
  void    SendMessage(const Message &message);
  Message GetMessage();
  void    BlockUntilMessages();
};
} // namespace kronk