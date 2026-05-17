#include "MessageQueue.hpp"

namespace kronk
{
void MessageQueue::SendMessage(const Message &message)
{
  // std::unique_lock lock{mQueueMutex};
  mMessages.emplace_back(message);
  // mHasMessages.notify_one();
}

Message MessageQueue::GetMessage()
{
  // std::unique_lock lock{mQueueMutex};
  Message message = mMessages.front();
  mMessages.pop_front();
  return message;
}

void MessageQueue::BlockUntilMessages()
{
  // TODO: enable once we multi thread
#if 0
  std::unique_lock lock{mQueueMutex};
  mHasMessages.wait(lock, [this]() { return !mMessages.empty(); });
#endif
}

} // namespace kronk