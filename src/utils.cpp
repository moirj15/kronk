#include "utils.hpp"

#include <cassert>
namespace io
{
std::string ReadFile(const std::string &path)
{
  FilePtr file{fopen(path.data(), "rb")};
  assert(file);

  fseek(file.get(), 0, SEEK_END);
  u64 length = ftell(file.get());
  rewind(file.get());
  assert(length > 0);

  std::string data(length, 0);

  fread(data.data(), sizeof(u8), length, file.get());

  return data;
}
} // namespace io