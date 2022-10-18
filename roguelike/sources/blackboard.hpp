#pragma once

#include <string>
#include <unordered_map>
#include <vector>
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
class NamedDataPool
{
public:

  DataType& access(size_t idx) { return data[idx]; }
  const DataType& access(size_t idx) const { return data.at(idx); }

  void expand(size_t max_idx) { if (max_idx >= data.size()) data.resize(max_idx + 1); }

private:
  std::vector<DataType> data;
};

}

class Blackboard
  : public detail::NamedDataPool<float>
  , public detail::NamedDataPool<int>
  , public detail::NamedDataPool<flecs::entity>
  , public detail::NamedDataPool<glm::ivec2>
{
public:
  static size_t getId(std::string_view name)
  {
    return detail::name_map.getId(name);
  }

  template<typename DataType>
  void set(size_t idx, const DataType &in_data)
  {
    detail::NamedDataPool<DataType>::expand(idx);
    detail::NamedDataPool<DataType>::access(idx) = in_data;
  }

  template<typename DataType>
  DataType get(size_t idx) const
  {
    return detail::NamedDataPool<DataType>::access(idx);
  }
};