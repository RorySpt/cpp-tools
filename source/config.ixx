module;
#include <filesystem>
#include <string_view>

#include "udp_receiver.h"
export module config;

import utils;


constexpr std::string_view config_file_name = "config.json";

struct Config
{
    std::string app_name;
    std::string app_full_name;
    std::string app_dir;
    std::string app_path;

    Config() = default;
    Config(int argc, char** argv);
};



Config::Config(int argc, char** argv)
{
    Config& config = *this;
    config.app_path = argv[0];

    if (size_t index;
        (index = config.app_path.find_last_of('\\')) != std::string::npos
        || (index = config.app_path.find_last_of('/')) != std::string::npos
        )
    {
        config.app_dir = config.app_path.substr(0, index);
        config.app_full_name = config.app_path.substr(index + 1, config.app_path.size() - index + 1);
        const auto dot_index = config.app_full_name.find_last_of('.');
        if (dot_index != std::string::npos)
            config.app_name = config.app_full_name.substr(0, dot_index);
        else
            config.app_name = config.app_full_name;
    }

}


export struct WatchDogConfig : Config
{
    udp_receiver::socket_create_parameters udp_receiver_parameters;
    udp_sender::socket_create_parameters udp_reply_parameters;
    process::process_launch_parameters process_launch_parameters;

    bool enable_watchdog = false;
    double hung_wait_time = 0;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WatchDogConfig, udp_receiver_parameters, udp_reply_parameters, process_launch_parameters, enable_watchdog, hung_wait_time)

        WatchDogConfig() = default;
    WatchDogConfig(int argc, char** argv) :Config(argc, argv)
    {
        auto& config = *this;
        auto const config_path = config.app_dir + "\\" + config_file_name.data();
        {
            //auto& config = *this;
            //auto const config_path = config.app_dir + "\\" + config_file_name.data();

            deserialize_from_file(config, config_path);
            config.process_launch_parameters.app_path = std::filesystem::absolute(config.process_launch_parameters.app_path).string();
        }
    }
};

WatchDogConfig g_config;
export void init_config(int argc, char** argv)
{
    g_config = WatchDogConfig(argc, argv);
}
export WatchDogConfig& get_config()
{
    return g_config;
}
