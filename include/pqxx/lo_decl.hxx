#ifndef PQXX_H_LO
#define PQXX_H_LO
#include "pqxx/internal/compiler-internal-pre.hxx"

#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <vector>

/// --- DECLARATION ---
namespace pqxx {
  template<typename T>
  concept single_byted = requires { sizeof(T) == 1; };

  template <single_byted T>
  struct lo_iterator;

  template <single_byted T>
  struct lo {
    friend struct lo_iterator<T>;

    using iterator = lo_iterator<T>;

    explicit lo(transaction_base& t, oid o, int fd);

    // Direct mappings to libpq lo_* functions. May invalidate iterators if used directly
    int read(char *buf, size_t len);
    int write(const char* buf, size_t len);
    int64_t seek(int offset, int whence = SEEK_SET);
    int64_t position();

    lo_iterator<T> begin();
    lo_iterator<T> end();

    void write(const std::string_view& data);

    template <std::contiguous_iterator Begin,
              std::contiguous_iterator End>
    void write(const Begin& begin, const End& end);

    size_t size();
    
  private:
    transaction_base& t_;
    oid oid_;
    int fd_;
    int64_t size_;  // Cached size for iterator

    pqxx::internal::pq::PGconn* raw_connection();
  };

  template <single_byted T>
  struct lo_iterator {
    using value_type = T;
    using difference_type = int64_t;
    using iterator_category = std::bidirectional_iterator_tag;

    using lo_position_type = size_t;

    template <single_byted _T>
    friend lo_iterator<_T> operator-(const typename lo_iterator<_T>::difference_type& n, const lo_iterator<_T>& iter);
  
    template <single_byted _T>
    friend lo_iterator<_T> operator+(const typename lo_iterator<_T>::difference_type& n, const lo_iterator<_T>& iter);

    static constexpr const size_t max_window_size = 4096;

    lo_iterator(lo<T>& obj, int64_t position) noexcept;
    lo_iterator(const lo_iterator& other);
    lo_iterator(lo_iterator&& other) noexcept;
    lo_iterator(); // Required by std iterator concepts; constructs end iterator

    lo_iterator& operator=(const lo_iterator& other);
    lo_iterator& operator=(lo_iterator&& other);

    lo_iterator& operator++();
    lo_iterator operator++(int);
    lo_iterator& operator--();
    lo_iterator operator--(int);
    bool operator==(const lo_iterator& other) const;
    auto operator<=>(const lo_iterator& other) const;

    difference_type operator-(const lo_iterator& other) const;
    difference_type operator+(const lo_iterator& other) const;
    lo_iterator& operator-=(const difference_type& d) const;
    lo_iterator& operator+=(const difference_type& d) const;
    lo_iterator operator-(const difference_type& other) const;
    lo_iterator operator+(const difference_type& other) const;

    const T& operator*() const;
    const T& operator[](const difference_type& d) const;
  private:
    lo<T>* obj_;
    std::optional<std::vector<T>> window_;

    // Window's position in large object
    int64_t window_pos_;
    // Position of current T element in large object
    size_t lo_pos_;

    // libpq limits position to int64 capacity; use next unsigned value as end indicator.
    // This will simplify ordered comparison operators.
    static constexpr const size_t end_pos_value = size_t{std::numeric_limits<int64_t>::max()} + 1;

    size_t pos_in_window() const;
    void set_end_state();
    void load_window(int64_t lo_position);
  };

  template <single_byted T>
  lo_iterator<T> operator-(const typename lo_iterator<T>::difference_type& n, const lo_iterator<T>& iter);
  
  template <single_byted T>
  lo_iterator<T> operator+(const typename lo_iterator<T>::difference_type& n, const lo_iterator<T>& iter);
}

#include "pqxx/internal/compiler-internal-post.hxx"
#endif
