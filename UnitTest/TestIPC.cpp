#include "../EasyIPC.h"
#include <iostream>
int main()
{  //
    using namespace EasyIPC;
    auto MsgQueueName = "/MyMsgQueue";

    {
        using MQ = MessageQueue<POSIX>;

        auto Sender = MQ::Sender::Bind( MsgQueueName );
        auto Receiver = MQ::Receiver::Bind( MsgQueueName );

        Sender.Send( "[hello POSIX message world, from]" );

        std::cout << Receiver.Receive().value_or( "No More Msg" ) << '\n';
        std::cout << Receiver.Receive().value_or( "No More Msg" ) << '\n';
    }
    std::cout << "\n---------------------------------------------------------\n\n";
    {
        using MQ = MessageQueue<SystemV>;

        auto Sender = MQ::Sender::Bind( MsgQueueName );
        auto Receiver = MQ::Receiver::Bind( MsgQueueName );

        Sender.Send( "[hello System V message world]" );
        Sender.Send( "[hello System V message world again]" );

        std::cout << Receiver.Receive().value_or( "No More Msg" ) << '\n';
        std::cout << Receiver.Receive().value_or( "No More Msg" ) << '\n';
    }

    return 0;
}