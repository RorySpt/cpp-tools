#pragma once
#undef UNICODE

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
//#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>

#include <mutex>
#include <optional>
#include <queue>
#include <ranges>


#include <thread>
#include "json.hpp"
#include "print.h"
#include "thread_pool.h"

#pragma comment (lib, "Ws2_32.lib")
class WSAStartupGuard
{
public:
    WSAStartupGuard()
    {
        result_code = WSAStartup(static_cast<WORD>((2U << 8) + 2U), &wsa_data);
        if (result_code != 0)
        {
            printf("WSAStartup failed with error: %d\n", result_code);
        }
    }
    ~WSAStartupGuard()
    {
        if (IsValid())
            WSACleanup();
    }
    bool IsValid() const
    {
        return result_code == 0;
    }
    int result_code = -1;
    WSADATA wsa_data;
};
static void wsa_startup()
{
    thread_local WSAStartupGuard Guard;
}
class udp_receiver
{
public:

    struct socket_create_parameters
    {
        bool non_blocking = false;
        bool broadcast = false;
        bool group = true;
        std::string group_addr = "224.0.1.4";
        std::string listen_ip = "0.0.0.0";
        u_short listen_port = 7880;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(socket_create_parameters, non_blocking, broadcast, group, group_addr, listen_ip, listen_port)
    };
    //NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(socket_create_parameters, non_blocking, group, group_addr, listen_ip, listen_port)

    using listen_function = std::function<void(const std::vector<char>&)>;
    using listen_object = std::unique_ptr<listen_function>;


    std::jthread listen_thread;
    SOCKET socket;

    std::mutex listeners_mutex;
    std::map<size_t, listen_function> listeners;
    size_t id_count = 0;

    std::mutex message_queue_mutex;
    std::queue<std::vector<char>> message_queue;
    size_t message_queue_max = 32768; // 2^15
    size_t discard_count = 0;

    std::condition_variable add_message_cv;

    udp_receiver(const socket_create_parameters& parameters = {})
    {
        try
        {
            socket = create_listen_socket(parameters);
        }
        catch (std::exception& exception)
        {
            println("{},{}", exception.what(), WSAGetLastError());
            
            exit(-1);
        }


        listen_thread = std::jthread([&](const std::stop_token& stop_token)
            {
                /*std::array<char, 8192> buffer;*/
                std::vector<char> buffer;
                buffer.resize(35768);
                while (!stop_token.stop_requested())
                {
                    sockaddr_in sender_address;
                    int sender_address_size = sizeof(sockaddr);
                    const auto n = recvfrom(socket, buffer.data(), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&sender_address), &sender_address_size);

                    if (n > 0)
                    {
                        //thread_pool.submit([this, message = std::vector<char>(buffer.data(), buffer.data() + n)]()
                        //    {
                        //        
                        //    });
                        std::vector<listen_function> _listeners;
                        {
                            std::lock_guard guard(listeners_mutex);
                            auto value_view = std::ranges::views::values(listeners);
                            _listeners.assign(value_view.begin(), value_view.end());
                        }
                        for (auto& listener : _listeners)
                        {
                            listener({ buffer.data(), buffer.data() + n });
                        }
                    }else
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            });

        add_listener([&](const std::vector<char>& message)
            {
                {
                    std::lock_guard guard(message_queue_mutex);
                    message_queue.push(message);
                }

                add_message_cv.notify_one();

                if (message_queue.size() > message_queue_max)
                {
                    std::lock_guard guard(message_queue_mutex);
                    while (message_queue.size() > message_queue_max)
                    {
                        message_queue.pop();
                        ++discard_count;
                    }
                }

            });
    }
    // 查询，如果有消息取出消息
    std::optional<std::vector<char>> peek_message()
    {
        std::lock_guard guard(message_queue_mutex);
        if (!message_queue.empty())
        {
            auto message = std::move(message_queue.front());
            message_queue.pop();
            return message;
        }
        return {};
    }
    std::vector<std::vector<char>> peek_all_message()
    {
        std::vector<std::vector<char>> messages;
        if (!message_queue.empty())
        {
            std::lock_guard guard(message_queue_mutex);
            while (!message_queue.empty())
            {
                messages.emplace_back(std::move(message_queue.front()));
                message_queue.pop();
            }
        }
        return messages;
    }
    // wait until message existed, then take all
    std::vector<char> get_message()
    {

        if (!message_queue.empty())
        {
            std::lock_guard guard(message_queue_mutex);
            auto message = std::move(message_queue.front());
            message_queue.pop();
            return message;
        }
        else
        {
            std::unique_lock lock(message_queue_mutex);
            add_message_cv.wait(lock, [&]()
                {
                    return !message_queue.empty();
                });
            auto message = std::move(message_queue.front());
            message_queue.pop();
            return message;
        }
    }
    // if has message, take all
    std::vector<std::vector<char>> get_all_message()
    {
        std::vector<std::vector<char>> messages;
        if (!message_queue.empty())
        {
            std::lock_guard guard(message_queue_mutex);
            while (!message_queue.empty())
            {
                messages.emplace_back(std::move(message_queue.front()));
                message_queue.pop();
            }
        }
        else
        {
            std::unique_lock lock(message_queue_mutex);
            add_message_cv.wait(lock, [&]()
                {
                    return !message_queue.empty();
                });
            while (!message_queue.empty())
            {
                messages.emplace_back(std::move(message_queue.front()));
                message_queue.pop();
            }
        }
        return messages;
    }
private:
    size_t add_listener(const listen_function& listen)
    {
        std::lock_guard guard(listeners_mutex);
        auto id = id_count++;
        listeners.emplace(id, listen);
        return id;
    }
    void remove_listener(size_t id)
    {
        listeners.erase(id);
    }

    static void set_no_block(SOCKET socket, bool b_cond)
    {
        unsigned long on = b_cond;
        if (ioctlsocket(socket, FIONBIO, &on))
        {
            throw(std::exception(std::format("disabled block failed!! ErrorCode: {}", WSAGetLastError()).c_str()));
        }
    }
    static void set_join_to_group(SOCKET socket, std::string_view multiaddr)
    {
        ip_mreq mreq = {};
        mreq.imr_multiaddr.S_un.S_addr = inet_addr(multiaddr.data()); //组播源地址
        mreq.imr_interface.S_un.S_addr = INADDR_ANY;       //本地地址	// 将要添加到多播组的 IP，类似于 成员号
        const int result = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*> (&mreq), sizeof(mreq));	//关键函数，用于设置组播、链接组播。此时套接字client已经连上组播
        if (result == SOCKET_ERROR)
        {
            throw(std::exception(std::format("join group failed!! ErrorCode: {}", WSAGetLastError()).c_str()));
        }
    }
    static void set_broadcast(SOCKET socket, bool b)
    {
        const int result = setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*> (&b), sizeof(b));	//关键函数，用于设置组播、链接组播。此时套接字client已经连上组播
        if (result == SOCKET_ERROR)
        {
            throw(std::exception(std::format("set broadcast {} failed!! ErrorCode: {}", b, WSAGetLastError()).c_str()));
        }
    }
    static SOCKET create_listen_socket(const socket_create_parameters& parameters = {})
    {
        wsa_startup();
        const SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket == INVALID_SOCKET)
        {
            throw(std::exception("create socket failed!!"));
        }

        sockaddr_in listen_address_in;
        listen_address_in.sin_family = AF_INET;
        listen_address_in.sin_port = htons(parameters.listen_port);
        listen_address_in.sin_addr.s_addr = /*parameters.group ? INADDR_ANY : */inet_addr(parameters.listen_ip.c_str());
        const auto result = bind(socket, reinterpret_cast<sockaddr*>(&listen_address_in), sizeof(sockaddr));


        if (result != 0)
        {
            throw(std::exception(std::format("bind failed!! ErrorCode: {}", WSAGetLastError()).c_str()));
        }
        if (parameters.group)
        {
            set_join_to_group(socket, parameters.group_addr);
        }
        if (parameters.broadcast)
        {
            set_broadcast(socket, true);
        }
        if (1||parameters.non_blocking)
        {
            set_no_block(socket, true);
        }


        return socket;
    }
};


class udp_sender
{
public:
    struct socket_create_parameters
    {
        bool broadcast = false;
        bool group = true;
        std::string group_addr = "224.0.1.4";
        std::string send_ip = "224.0.1.4";
        u_short send_port = 7880;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(socket_create_parameters, broadcast, group, group_addr, send_ip, send_port)
    };


    SOCKET udp_socket;

    std::function<int(const std::vector<char>& message)> sender;
    udp_sender(const socket_create_parameters& parameters = {})
    {
        wsa_startup();
        try
        {
            udp_socket = create_send_socket(parameters);
        }
        catch (std::exception& exception)
        {
            println("{},{}", exception.what(), WSAGetLastError());
            abort();
        }
        sockaddr_in send_address;
        send_address.sin_family = AF_INET;
        send_address.sin_port = htons(parameters.send_port);
        send_address.sin_addr.s_addr = parameters.broadcast
    		? ADDR_ANY : inet_addr(parameters.group 
                ? parameters.group_addr.c_str() : parameters.send_ip.c_str());

        sender = [send_address, this](const std::vector<char>& message)
        {
            return sendto(udp_socket, message.data(), static_cast<int>(message.size()), 0, reinterpret_cast<const sockaddr*>(&send_address), sizeof(sockaddr));
        };
    }
    ~udp_sender()
    {
        closesocket(udp_socket);
    }
    
    static SOCKET create_send_socket(const socket_create_parameters& parameters = {})
    {
        wsa_startup();
        const SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket == INVALID_SOCKET)
        {
            throw(std::exception("create socket failed!!"));
        }
        if (parameters.group)
        {
            set_join_to_group(socket, parameters.group_addr);
        }
        if (parameters.broadcast)
        {
            set_broadcast(socket, true);
        }
        return socket;
    }
    [[maybe_unused]] int send(const std::vector<char>& message) const
    {
        return sender(message);
    }
    static void set_join_to_group(SOCKET socket, std::string_view multiaddr)
    {
        ip_mreq mreq = {};
        mreq.imr_multiaddr.S_un.S_addr = inet_addr(multiaddr.data());
        mreq.imr_interface.S_un.S_addr = INADDR_ANY;      
        const int result = setsockopt(socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*> (&mreq), sizeof(mreq));	//关键函数，用于设置组播、链接组播。此时套接字client已经连上组播
        if (result == SOCKET_ERROR)
        {
            throw(std::exception(std::format("join group failed!! ErrorCode: {}", WSAGetLastError()).c_str()));
        }
    }
    static void set_broadcast(SOCKET socket, bool b)
    {
        const int result = setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*> (&b), sizeof(b));	//关键函数，用于设置组播、链接组播。此时套接字client已经连上组播
        if (result == SOCKET_ERROR)
        {
            throw(std::exception(std::format("set broadcast {} failed!! ErrorCode: {}", b, WSAGetLastError()).c_str()));
        }
    }
};