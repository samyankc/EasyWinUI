#ifndef _EASYIPC_H_
#define _EASYIPC_H_

#include <mqueue.h>  // POSIX MQ

#include <sys/ipc.h>  // System V MQ
#include <sys/msg.h>  // System V MQ

#include <string>
#include <string_view>
#include <optional>
#include <algorithm>

namespace EasyIPC
{

    namespace Archive
    {

        enum class OpenFlag : int {
            CloseOnExec = O_CLOEXEC,
            CreateIfNotExist = O_CREAT,
            DirectoryOnly = O_DIRECTORY,
            Exclusive = O_EXCL,
            ExclusiveCreate = O_EXCL | O_CREAT,
            NoControllerTTY = O_NOCTTY,
            NoFollowSymLink = O_NOFOLLOW,
            Truncate = O_TRUNC,
            Append = O_APPEND,
            SyncData = O_DSYNC,
            NonBlocking = O_NONBLOCK,
            SyncRead = O_RSYNC,
            SyncFile = O_SYNC,
            ReadOnly = O_RDONLY,
            ReadWrite = O_RDWR,
            WriteOnly = O_WRONLY
        };
    }
    enum AccessRight : mode_t { RW_RW_R__ = 0664 };

    inline namespace MessageQueueModel
    {
        struct POSIX
        {
            using Descriptor = mqd_t;
            using Attribute = struct mq_attr;
            using Permission = AccessRight;

            inline constexpr static auto InvalidDescriptor = static_cast<Descriptor>( -1 );
            inline constexpr static auto InvalidSize = static_cast<ssize_t>( -1 );
            inline constexpr static auto DefaultPriority = static_cast<unsigned int>( 1 );

            inline static auto Close = mq_close;

            inline static auto Open( std::string_view QueueName, int Flag, auto... Args ) -> std::optional<Descriptor>
            {
                auto Result = mq_open( QueueName.data(), Flag, Args... );
                if( Result == InvalidDescriptor ) return std::nullopt;
                return Result;
            }

            inline static auto OpenForWrite( std::string_view QueueName )
            {
                return Open( QueueName, O_CREAT | O_WRONLY, Permission::RW_RW_R__, nullptr );
            }

            inline static auto OpenForRead( std::string_view QueueName )  //
            {
                return Open( QueueName, O_RDONLY | O_NONBLOCK );
            }

            inline static auto Send( Descriptor MQD, std::string_view MSG )
            {
                return mq_send( MQD, MSG.data(), MSG.length() + 1, DefaultPriority );
            }

            inline static auto MaxMsgLength( Descriptor QD )
            {
                mq_attr Attr;
                mq_getattr( QD, &Attr );
                return Attr.mq_msgsize;
            }

            inline static auto Receive( Descriptor QD ) -> std::optional<std::string>
            {
                const auto BufferSize = MaxMsgLength( QD );
                char Buffer[BufferSize];
                if( mq_receive( QD, Buffer, BufferSize, nullptr ) == InvalidSize ) return std::nullopt;
                return std::string{ Buffer };
            }
        };

        struct SystemV
        {
            using Descriptor = int;
            using QueueKey = key_t;
            using Permission = AccessRight;

            inline constexpr static auto InvalidDescriptor = static_cast<Descriptor>( -1 );
            inline constexpr static auto InvalidKey = static_cast<QueueKey>( -1 );
            inline constexpr static auto InvalidSize = static_cast<ssize_t>( -1 );
            inline constexpr static auto DefaultProjection = 'P';
            inline constexpr static auto DefaultProxyBufferSize = 8192;
            inline constexpr static auto DefaultProxyBufferMType = 1;
            inline constexpr static auto DefaultQueueNamePrefix = "/dev/shm";

            struct ProxyBuffer
            {
                long mtype;
                char mtext[DefaultProxyBufferSize];
                ProxyBuffer( long mtype ) : mtype{ mtype } {}
            };

            inline static auto Close( Descriptor MQD ) { return msgctl( MQD, IPC_RMID, nullptr ); };

            inline static auto GetKey( std::string_view QueueName, char Projection = DefaultProjection )
            {
                return ftok( QueueName.data(), Projection );
            }

            inline static auto Open( std::string_view QueueName, int ExtraFlag = 0 ) -> std::optional<Descriptor>
            {
                auto QueueNameMakeCMD = std::string{ "touch " }.append( DefaultQueueNamePrefix ).append( QueueName );
                system( QueueNameMakeCMD.c_str() );

                // what the hack
                auto ModifiedQueueName = QueueNameMakeCMD.data() + 6;
                auto Result = msgget( GetKey( ModifiedQueueName ), Permission::RW_RW_R__ | ExtraFlag );

                if( Result == InvalidDescriptor ) return std::nullopt;
                return Result;
            }

            inline static auto OpenForWrite( std::string_view QueueName ) { return Open( QueueName, IPC_CREAT ); }
            inline static auto OpenForRead( std::string_view QueueName ) { return Open( QueueName ); }

            inline static auto Send( Descriptor QD, std::string_view MSG )
            {
                auto Buff = ProxyBuffer{ DefaultProxyBufferMType };
                if( MSG.length() >= sizeof( Buff.mtext ) ) return 0;

                std::copy_n( MSG.data(), MSG.length(), Buff.mtext );
                Buff.mtext[MSG.length()] = '\0';
                return msgsnd( QD, &Buff, MSG.length() + 1, IPC_NOWAIT );
            }

            inline static auto Receive( Descriptor QD ) -> std::optional<std::string>
            {
                auto Buff = ProxyBuffer{ DefaultProxyBufferMType };
                auto LoadedSize = msgrcv( QD, &Buff, sizeof( Buff.mtext ), 0, IPC_NOWAIT );
                if( LoadedSize == InvalidSize ) return std::nullopt;
                return std::string( Buff.mtext, LoadedSize );
            }
        };
    }  // namespace MessageQueueModel

    template<typename Model = MessageQueueModel::POSIX>
    struct MessageQueue
    {
        using Descriptor = Model::Descriptor;

        struct Messenger
        {
            Messenger( std::optional<Descriptor> QueueDescriptor ) : QueueDescriptor{ QueueDescriptor } {}
            virtual ~Messenger() { QueueDescriptor.transform( Model::Close ); }

          protected:
            std::optional<Descriptor> QueueDescriptor;
        };

        struct Sender : Messenger
        {
            Sender( std::string_view QueueName ) : Messenger{ Model::OpenForWrite( QueueName ) } {}
            static auto Bind( std::string_view QueueName ) { return Sender{ QueueName }; }

            auto Send( std::string_view Message ) const
            {
                return this->QueueDescriptor.transform( [=]( Descriptor QD ) { return Model::Send( QD, Message ); } );
            }
        };

        struct Receiver : Messenger
        {
            Receiver( std::string_view QueueName ) : Messenger{ Model::OpenForRead( QueueName ) } {}
            static auto Bind( std::string_view QueueName ) { return Receiver{ QueueName }; }

            auto Receive() const { return this->QueueDescriptor.and_then( Model::Receive ); }
        };
    };
}  // namespace EasyIPC
#endif