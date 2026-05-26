#pragma once

#include "utils.hpp"

#include <deque>
#include <glm/glm.hpp>
#include <variant>

namespace kronk
{
enum class MessageType : u32
{
  Invalid,
  ClearScreen,
  SubmitUnit,
  LoadAsset,
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

enum class Asset : u32
{
  GroundsKeeper,
  Goose,
};

struct LoadAsset
{
  MessageType type{MessageType::LoadAsset};
  Asset       asset{};
};

using Message = std::variant<ClearScreen, SubmitUnit, LoadAsset>;

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