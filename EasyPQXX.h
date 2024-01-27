#ifndef H_EASY_PQXX_
#define H_EASY_PQXX_
#include <pqxx/pqxx>

namespace SQL = pqxx;

namespace pqxx  // inject helper class
{
    template<size_t ParameterCount, size_t ExpectedRowCount = std::numeric_limits<size_t>::max()>
    struct PreparedStatment
    {
        SQL::zview Name;
        SQL::connection& AssociatedConnection;

        PreparedStatment() = delete;
        PreparedStatment( const PreparedStatment& ) = delete;

        PreparedStatment( SQL::zview Name, SQL::connection& Conn, SQL::zview Statement ) : Name{ Name }, AssociatedConnection{ Conn }
        {
            AssociatedConnection.prepare( Name, Statement );
        }

        template<typename... Ts>
        auto operator()( Ts&&... Args ) const  //
            requires( sizeof...( Args ) == ParameterCount )
        {
            auto Tx = SQL::work{ AssociatedConnection };
            auto TxCommitGuard = std::unique_ptr<decltype( Tx ), decltype( []( auto* P ) { P->commit(); } )>( &Tx );

            if constexpr( ExpectedRowCount == 0 ) return Tx.exec_prepared0( Name, std::forward<Ts>( Args )... );
            if constexpr( ExpectedRowCount == 1 ) return Tx.exec_prepared1( Name, std::forward<Ts>( Args )... );
            if constexpr( ExpectedRowCount == std::numeric_limits<size_t>::max() ) return Tx.exec_prepared( Name, std::forward<Ts>( Args )... );
        }
    };
}  // namespace pqxx
#endif