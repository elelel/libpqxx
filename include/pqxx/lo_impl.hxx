#ifndef PQXX_H_LO_IMPL
#define PQXX_H_LO_IMPL

#include <iostream>

#include "pqxx/internal/compiler-internal-pre.hxx"

/// --- IMPLEMENTATION ---

#include "pqxx/internal/gates/connection-lo.hxx"

/// --- Large object (lo) class implementation ---

template <pqxx::single_byted T>
inline pqxx::lo<T>::lo(pqxx::transaction_base& t, oid o, int fd) :
  oid_{o},
  t_{t},
  fd_{fd} {
}

template <pqxx::single_byted T>
inline auto pqxx::lo<T>::begin() -> lo_iterator<T> {
  return lo_iterator<T>(*this, 0);
}

template <pqxx::single_byted T>
inline auto pqxx::lo<T>::end() -> lo_iterator<T> {
  return lo_iterator<T>();
}

template <pqxx::single_byted T>
inline size_t pqxx::lo<T>::size() {
  auto conn = raw_connection();
  const auto pos = position();
  const auto size = lo_lseek64(conn, fd_, 0, SEEK_END);
  lo_lseek64(conn, fd_, pos, SEEK_SET);
  return size;
}

template <pqxx::single_byted T>
inline int pqxx::lo<T>::read(char *buf, size_t len) {
  return ::lo_read(raw_connection(), fd_, buf, len);
}

template <pqxx::single_byted T>
inline int pqxx::lo<T>::write(const char *buf, size_t len) {
  return ::lo_write(raw_connection(), fd_, buf, len);
}

template <pqxx::single_byted T>
inline int64_t pqxx::lo<T>::position() {
  return lo_tell64(raw_connection(), fd_);
}

template <pqxx::single_byted T>
inline int64_t pqxx::lo<T>::seek(int offset, int whence) {
  return lo_lseek64(raw_connection(), fd_, offset, whence);
}


template <pqxx::single_byted T>
inline void pqxx::lo<T>::write(const std::string_view& data) {
  write(data.begin(), data.end());
}

template <pqxx::single_byted T>
template <std::contiguous_iterator Begin,
          std::contiguous_iterator End>
inline void pqxx::lo<T>::write(const Begin& begin, const End& end) {
  static_assert(std::is_same_v<typename Begin::value_type, typename End::value_type>);
  static_assert(single_byted<typename Begin::value_type>);
  const auto ptr = reinterpret_cast<const char*>(&*begin);
  const auto len = reinterpret_cast<uintptr_t>(&*(end)) -
    reinterpret_cast<uintptr_t>(ptr);
  const auto written_sz = write(ptr, len);
  if (written_sz == len) return;
  if (written_sz < 0) 
    throw std::runtime_error("Failed to write to large object, libpq signals error");
  else
    throw std::runtime_error("Failed to write_to large_object, " +
                             std::to_string(written_sz) + "/" + std::to_string(len) +
                             " bytes written");
}

template <pqxx::single_byted T>
inline pqxx::internal::pq::PGconn* pqxx::lo<T>::raw_connection() {
  return pqxx::internal::gate::connection_lo{t_.conn()}
    .raw_connection();
}

/// --- Iterator class implementation ---
template <pqxx::single_byted T>
inline pqxx::lo_iterator<T>::lo_iterator(lo<T>& obj, int64_t position) noexcept :
  obj_{&obj},
  window_{std::nullopt},
  lo_pos_{position >= 0 ? size_t(position) : 0},
  window_pos_{0} {
  load_window(lo_pos_);
}

template <pqxx::single_byted T>
inline pqxx::lo_iterator<T>::lo_iterator(const lo_iterator<T>& other) :
  obj_{other.obj_},
  window_{other.window_},
  lo_pos_{other.lo_pos_},
  window_pos_{other.window_pos_} {
}

template <pqxx::single_byted T>
inline pqxx::lo_iterator<T>::lo_iterator(lo_iterator<T>&& other) noexcept :
  obj_{std::move(other.obj_)},
  window_{std::move(other.window_)},
  lo_pos_{std::move(other.lo_pos_)},
  window_pos_{std::move(other.window_pos_)} {
}

template <pqxx::single_byted T>
inline pqxx::lo_iterator<T>::lo_iterator() :
  obj_{nullptr},
  window_{std::nullopt},
  lo_pos_{end_pos_value},
  window_pos_{0} {
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator=(const lo_iterator& other) -> lo_iterator& {
  obj_ = other.obj_;
  window_ = other.window_;
  lo_pos_ = other.lo_pos_;
  window_pos_ = other.window_pos_;
  return *this;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator=(lo_iterator&& other) -> lo_iterator& {
  obj_ = std::move(other.obj_);
  window_ = std::move(other.window_);
  lo_pos_ = std::move(other.lo_pos_);
  window_pos_ = std::move(other.window_pos_);
  return *this;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator++() -> lo_iterator<T>& {
  if (lo_pos_ != end_pos_value) {
    ++lo_pos_;
    if (pos_in_window() > window_->size() - 1)
      load_window(window_pos_ + window_->size());
  }
  return *this;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator++(int) -> lo_iterator<T> {
  lo_iterator<T> tmp(*this);
  this->operator++();
  return tmp;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator--() -> lo_iterator<T>& {
  if (lo_pos_ != end_pos_value) {
    if (pos_in_window() > 0) {
      --lo_pos_;
    } else {
      load_window(window_pos_ - window_->size());
      --lo_pos_;
    }
  } else {
    const auto obj_sz = obj_->size();
    load_window(window_pos_ - window_->size());
    lo_pos_ = obj_->position() - 1;
  }
  return *this;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator--(int) -> lo_iterator<T> {
  lo_iterator<T> tmp(*this);
  this->operator--();
  return tmp;
}


template <pqxx::single_byted T>
inline bool pqxx::lo_iterator<T>::operator==(const lo_iterator<T>& other) const {
  return lo_pos_ == other.lo_pos_;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator<=>(const lo_iterator<T>& other) const {
  return lo_pos_ <=> other.lo_pos_;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator-(const lo_iterator& other) const -> difference_type {
  return lo_pos_ - other.lo_pos_;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator+(const lo_iterator& other) const -> difference_type {
  return lo_pos_ + other.lo_pos_;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator-=(const difference_type& d) const -> lo_iterator& {
  return lo_pos_ - d;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator+=(const difference_type& d) const -> lo_iterator& {
  return lo_pos_ + d;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator-(const difference_type& d) const -> lo_iterator {
  lo_iterator tmp{*this};
  tmp.lo_pos_ -= d;
  return tmp;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator+(const difference_type& d) const -> lo_iterator {
  lo_iterator tmp{*this};
  tmp.lo_pos_ += d;
  return tmp;
}

template <pqxx::single_byted T>
inline auto pqxx::lo_iterator<T>::operator*() const -> const T& {
  return (*window_)[pos_in_window()];
}

template <pqxx::single_byted T>
inline size_t pqxx::lo_iterator<T>::pos_in_window() const {
  return lo_pos_ - window_pos_;
}

template <pqxx::single_byted T>
inline void pqxx::lo_iterator<T>::set_end_state() {
  // Transit to "end" iterator state
  if (window_.has_value()) window_->clear();
  lo_pos_ = end_pos_value;
  window_pos_ = 0;
}

template <pqxx::single_byted T>
inline void pqxx::lo_iterator<T>::load_window(int64_t lo_position) {
  if (!window_.has_value()) window_ = std::move(std::vector<T>{});
  if (window_->size() != max_window_size) window_->resize(max_window_size);
  obj_->seek(lo_position);
  auto bytes_read = obj_->read(reinterpret_cast<char*>(window_->data()), window_->size());
  if (bytes_read < window_->size()) {
    if (bytes_read > 0) {
      window_->resize(bytes_read);
    } else if (bytes_read == 0) {
      set_end_state();
    } else {
      // FIXME: No suitable exception in except.hxx; must define a new one
      throw std::runtime_error{"Read error in large object iterator while reading OID " +
          std::to_string(obj_->oid_) + " at position " + std::to_string(lo_position)};
    }
  }
  window_pos_ = lo_position;
}

template <pqxx::single_byted T>
pqxx::lo_iterator<T> pqxx::operator-(const typename lo_iterator<T>::difference_type& n, const lo_iterator<T>& iter) {
  lo_iterator<T> tmp{iter};
  // tmp.lo_pos_ = n - iter.lo_pos_;
  return tmp;
}
  
template <pqxx::single_byted T>
pqxx::lo_iterator<T> pqxx::operator+(const typename lo_iterator<T>::difference_type& n, const lo_iterator<T>& iter) {
  lo_iterator<T> tmp{iter};
  // tmp.lo_pos_ = n + iter.lo_pos_;
  return tmp;
}
#include "pqxx/internal/compiler-internal-post.hxx"
#endif
