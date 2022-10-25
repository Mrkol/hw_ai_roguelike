#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <flecs.h>
#include <glm/glm.hpp>


namespace detail
{

inline struct NameMap
{
  size_t getId(std::string_view name)
  {
    std::string str(name);
    auto it = nameIndices.find(str);
    if (it == nameIndices.end())
    {
      it = nameIndices.emplace(str, nameIndices.size()).first;
    }

    return it->second;
  }

  std::unordered_map<std::string, size_t> nameIndices;
} name_map;

template<typename DataType>
struct NamedDataPool
{
  std::vector<std::optional<DataType>> data;
};

}

class Blackboard
  : private detail::NamedDataPool<float>
  , private detail::NamedDataPool<int>
  , private detail::NamedDataPool<flecs::entity>
  , private detail::NamedDataPool<glm::ivec2>
{
public:
  static size_t getId(std::string_view name)
  {
    return detail::name_map.getId(name);
  }

  template<typename DataType>
  void set(size_t idx, const DataType &in_data)
  {
    auto& d = static_cast<detail::NamedDataPool<DataType>*>(this)->data;
    if (d.size() <= idx)
      d.resize(idx + 1, std::nullopt);
    d[idx] = in_data;
  }

  template<typename DataType>
  void unset(size_t idx)
  {
    auto& d = static_cast<detail::NamedDataPool<DataType>*>(this)->data;
    if (idx < d.size())
      d[idx] = std::nullopt;
  }

  template<typename DataType>
  std::optional<DataType> get(size_t idx) const
  {
    auto& d = static_cast<const detail::NamedDataPool<DataType>*>(this)->data;
    return idx < d.size() ? d[idx] : std::nullopt;
  }
};
