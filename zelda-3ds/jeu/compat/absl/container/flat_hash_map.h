#pragma once
// Version 3DS d'absl::flat_hash_map : std::unordered_map avec le hachage d'absl (voir hash.h).
// Comme Abseil, les tables à clés std::string acceptent aussi std::string_view et const char*
// pour find, contains, count, at, operator[] et erase.

#include <absl/hash/hash.h>

#include <memory>
#include <type_traits>
#include <unordered_map>

namespace absl {

template <class K, class V, class Hash = absl::Hash<K>,
          class Eq = typename container_internal::EgaliteParDefaut<K>::type,
          class Allocator = std::allocator<std::pair<const K, V>>>
class flat_hash_map : public std::unordered_map<K, V, Hash, Eq, Allocator> {
  using Base = std::unordered_map<K, V, Hash, Eq, Allocator>;

  // Clé « équivalente » utilisable sans construire de K (string_view pour une clé string, etc.)
  template <class K2>
  static constexpr bool Autre = !std::is_same_v<std::remove_cvref_t<K2>, K> && std::is_constructible_v<K, const K2&> &&
                                requires { typename Hash::is_transparent; };

public:
  using Base::Base;
  using Base::at;
  using Base::erase;
  using Base::operator[];

  template <class K2>
    requires Autre<K2>
  V& operator[](const K2& cle) {
    auto it = this->find(cle);
    if (it != this->end()) return it->second;
    return Base::operator[](K(cle));
  }

  template <class K2>
    requires Autre<K2>
  V& at(const K2& cle) {
    auto it = this->find(cle);
    if (it == this->end()) return Base::at(K(cle));  // lève std::out_of_range
    return it->second;
  }

  template <class K2>
    requires Autre<K2>
  const V& at(const K2& cle) const {
    auto it = this->find(cle);
    if (it == this->end()) return Base::at(K(cle));
    return it->second;
  }

  template <class K2>
    requires Autre<K2>
  typename Base::size_type erase(const K2& cle) {
    auto it = this->find(cle);
    if (it == this->end()) return 0;
    Base::erase(it);
    return 1;
  }
};

template <class K, class V, class Hash = absl::Hash<K>,
          class Eq = typename container_internal::EgaliteParDefaut<K>::type,
          class Allocator = std::allocator<std::pair<const K, V>>>
using node_hash_map = flat_hash_map<K, V, Hash, Eq, Allocator>;

} // namespace absl
