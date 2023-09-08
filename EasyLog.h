#ifndef _EASYLOG_H
#define _EASYLOG_H

#include <pqxx/pqxx>
#include "EasyString.h"

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
            Tx.exec( "INSERT INTO cgi_execution_log(app_name,message) VALUES('{}','{}')"_FMT  //
                     ( ApplicationName, Message ) );
            Tx.commit();
        }
    };
}  // namespace EasyLog

#endif