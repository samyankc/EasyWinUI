#ifndef _EASYLOG_H
#define _EASYLOG_H

#include <format>
#include <pqxx/pqxx>

inline namespace EasyLog
{
    struct Database
    {
        std::string ApplicationName{ "Unspecified" };
        pqxx::connection Connection;

        Database( pqxx::zview ConnectionString ) : Connection{ ConnectionString } {}

        auto operator<<( std::string_view Message )
        {
            auto Tx = pqxx::work{ Connection };
            Tx.exec( std::format(                                                     //
                "INSERT INTO cgi_execution_log(app_name,message) VALUES('{}','{}')",  //
                ApplicationName, Message ) );
            Tx.commit();
        }
    };
}  // namespace EasyLog

#endif