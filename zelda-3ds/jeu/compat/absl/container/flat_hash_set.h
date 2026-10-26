#pragma once
// Version 3DS d'absl::flat_hash_set : std::unordered_set avec le hachage d'absl (voir hash.h).

#include <absl/hash/hash.h>

#include <memory>
#include <unordered_set>

namespace absl {

template <class K, class Hash = absl::Hash<K>, class Eq = typename container_internal::EgaliteParDefaut<K>::type,
          class Allocator = std::allocator<K>>
using flat_hash_set = std::unordered_set<K, Hash, Eq, Allocator>;

template <class K, class Hash = absl::Hash<K>, class Eq = typename container_internal::EgaliteParDefaut<K>::type,
          class Allocator = std::allocator<K>>
using node_hash_set = std::unordered_set<K, Hash, Eq, Allocator>;

} // namespace absl
