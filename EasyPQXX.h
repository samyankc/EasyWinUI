#ifndef H_EASY_PQXX_
#define H_EASY_PQXX_
#include <pqxx/pqxx>
#include <format>

namespace SQL = pqxx;

namespace pqxx  // inject helper class
{

    template<size_t ParameterCount, size_t ExpectedRowCount = std::numeric_limits<size_t>::max()>
    struct PreparedStatement
    {
        static auto UniquePreparedStatmentName()
        {
            static size_t UniqueID = 1000;
            return std::format( "PreparedStatement_{}_{}_{}_", ParameterCount, ExpectedRowCount, ++UniqueID );
        }

        std::string Name;
        SQL::connection& AssociatedConnection;

        PreparedStatement() = delete;
        PreparedStatement( const PreparedStatement& ) = delete;

        PreparedStatement( SQL::connection& Conn, SQL::zview Statement ) : Name{ UniquePreparedStatmentName() }, AssociatedConnection{ Conn }
        {
            AssociatedConnection.prepare( Name, Statement );
        }

        template<typename... Ts>
        auto operator()( Ts&&... Args ) const  //
            requires( sizeof...( Args ) == ParameterCount )
        {
            auto Tx = SQL::work{ AssociatedConnection };
            auto TxCommitGuard = std::unique_ptr<decltype( Tx ), decltype( []( auto* P ) { P->commit(); } )>( &Tx );

            if constexpr( ExpectedRowCount == std::numeric_limits<size_t>::max() )
            {
                return Tx.exec_prepared( Name, std::forward<Ts>( Args )... );
            }

            if constexpr( ExpectedRowCount == 0 ) try
                {
                    return Tx.exec_prepared0( Name, std::forward<Ts>( Args )... );
                }
                catch( const SQL::unexpected_rows& )
                {
                    return SQL::result{};
                }

            if constexpr( ExpectedRowCount == 1 ) try
                {
                    return Tx.exec_prepared1( Name, std::forward<Ts>( Args )... );
                }
                catch( const SQL::unexpected_rows& )
                {
                    return SQL::row{};
                }
        }
    };
}  // namespace pqxx
#endif
