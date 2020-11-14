#include <string>

#include <pqxx/internal/callgate.hxx>
#include <pqxx/internal/libpq-forward.hxx>

namespace pqxx::internal::gate
{
class PQXX_PRIVATE connection_lo : callgate<connection>
{
  template <pqxx::single_byted T>
  friend struct pqxx::lo;

  template <pqxx::single_byted T>
  friend struct pqxx::lo_iterator;

  connection_lo(reference x) : super(x) {}

  pq::PGconn *raw_connection() const { return home().raw_connection(); }
};


class PQXX_PRIVATE const_connection_lo : callgate<connection const>
{
  template <pqxx::single_byted T>
  friend struct pqxx::lo;

  template <pqxx::single_byted T>
  friend struct pqxx::lo_iterator;

  const_connection_lo(reference x) : super(x) {}

  std::string error_message() const { return home().err_msg(); }
};
} // namespace pqxx::internal::gate
