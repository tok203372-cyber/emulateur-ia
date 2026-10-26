#pragma once
// Version 3DS simplifiée d'absl::Hash (Abseil n'est pas compilé pour la 3DS).
// Gère les types hachables par std::hash, les énumérations, les paires, les tuples, les chaînes
// et les types qui définissent AbslHashValue(H, const T&) (avec H::combine).

// En-têtes inclus indirectement par Abseil sur PC (le code du jeu en dépend parfois)
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace absl {

class HashState;

namespace hash_internal {

inline size_t melanger(size_t a, size_t b) noexcept {
  // mélange inspiré de boost::hash_combine (32 bits sur la 3DS)
  return a ^ (b + static_cast<size_t>(0x9E3779B9u) + (a << 6) + (a >> 2));
}

template <typename T>
struct EstPaireOuTuple : std::false_type {};
template <typename A, typename B>
struct EstPaireOuTuple<std::pair<A, B>> : std::true_type {};
template <typename... Ts>
struct EstPaireOuTuple<std::tuple<Ts...>> : std::true_type {};

template <typename T>
HashState hacher_un(HashState h, const T& valeur) noexcept;

} // namespace hash_internal

class HashState {
public:
  explicit HashState(size_t graine = 0x2545F491u) noexcept : mEtat(graine) {}

  static HashState combine(HashState h) noexcept { return h; }

  template <typename T, typename... Ts>
  static HashState combine(HashState h, const T& valeur, const Ts&... autres) noexcept {
    h = hash_internal::hacher_un(std::move(h), valeur);
    return combine(std::move(h), autres...);
  }

  static HashState combine_contiguous(HashState h, const unsigned char* donnees, size_t taille) noexcept {
    h.mEtat = hash_internal::melanger(h.mEtat, std::hash<std::string_view>{}(
        std::string_view(reinterpret_cast<const char*>(donnees), taille)));
    return h;
  }

  size_t valeur() const noexcept { return mEtat; }
  void ajouter(size_t v) noexcept { mEtat = hash_internal::melanger(mEtat, v); }

private:
  size_t mEtat;
};

namespace hash_internal {

template <typename T>
concept AAbslHashValue = requires(HashState h, const T& v) {
  { AbslHashValue(std::move(h), v) } -> std::same_as<HashState>;
};

template <typename T>
concept AStdHash = requires(const T& v) {
  { std::hash<T>{}(v) } -> std::convertible_to<size_t>;
};

template <typename T>
HashState hacher_un(HashState h, const T& valeur) noexcept {
  if constexpr (AAbslHashValue<T>) {
    return AbslHashValue(std::move(h), valeur);
  } else if constexpr (std::is_enum_v<T>) {
    using U = std::underlying_type_t<T>;
    h.ajouter(std::hash<U>{}(static_cast<U>(valeur)));
    return h;
  } else if constexpr (EstPaireOuTuple<T>::value) {
    return std::apply([&h](const auto&... e) { return HashState::combine(std::move(h), e...); }, valeur);
  } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {
    h.ajouter(std::hash<std::string_view>{}(std::string_view(valeur)));
    return h;
  } else {
    static_assert(AStdHash<T>, "type non hachable (absl::Hash version 3DS)");
    h.ajouter(std::hash<T>{}(valeur));
    return h;
  }
}

} // namespace hash_internal

template <typename T>
struct Hash {
  size_t operator()(const T& valeur) const noexcept {
    return hash_internal::hacher_un(HashState(), valeur).valeur();
  }
};

// Chaînes : recherche possible avec std::string_view ou const char* (comme Abseil)
template <>
struct Hash<std::string> {
  using is_transparent = void;
  size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
};
template <>
struct Hash<std::string_view> {
  using is_transparent = void;
  size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
};

template <typename... Ts>
size_t HashOf(const Ts&... valeurs) noexcept {
  return HashState::combine(HashState(), valeurs...).valeur();
}

namespace container_internal {
template <typename K>
struct EgaliteParDefaut {
  using type = std::equal_to<K>;
};
template <>
struct EgaliteParDefaut<std::string> {
  using type = std::equal_to<>;
};
template <>
struct EgaliteParDefaut<std::string_view> {
  using type = std::equal_to<>;
};
} // namespace container_internal

} // namespace absl
