module;
#include "code_trans.h"
#include "json.hpp"
#include "print.h"
#include <chrono>
#include <filesystem>

export module watch_dog;

import utils;
import config;
import display_impl;
auto& g_config = get_config();

export struct watch_dog
{
    watch_dog(int argc, char** argv)
    {
        init_config(argc, argv);
        println("using config: ");
        println(nlohmann::json(*config).dump(4));
        println("Will watch the {}", path_resolver(g_config.process_launch_parameters.app_path).file_full_name);
    }
    ~watch_dog()
    {
        if (config->enable_watchdog)
        {
            dlog.println("watch stopping, will kill the protect process \"{}\"", config->process_launch_parameters.app_path);
            const path_resolver resolver(config->process_launch_parameters.app_path);
            process::kill(resolver.file_full_name);
        }else
        {
            dlog.println("watch stopped!!");
        }
    }
    void tick()
    {
        if (config->enable_watchdog)
        {
            path_resolver resolver(config->process_launch_parameters.app_path);
            const auto process_infos = get_process_info(resolver.file_name);
            if (process_infos.empty())
            {
                dlog.println("can not find {}, will launch it", resolver.file_full_name);
                process::launch(config->process_launch_parameters);
                return;
            }
            if (process_infos.size() > 1)
            {
                dlog.println("find {} more than one, kill the more", resolver.file_full_name);
                for (size_t i = 1; i < process_infos.size(); ++i)
                {
	                const auto& process_info = process_infos[i];
                    const auto handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_info.pid);
                    TerminateProcess(handle, 0);
                }
            }
            if (get_has_window(process_infos[0]))
            {
                if (get_is_hung(process_infos[0]))
                {
                    if (IsHung == false)
                    {
                        IsHung = true;
                        timer0.reset();
                        dlog.println("{} is hung, wait {}s", resolver.file_full_name, config->hung_wait_time);
                    }
                    if (timer0.peek().count() > config->hung_wait_time)
                    {
                        dlog.println("hung timeout, will killed and relaunch", resolver.file_full_name);
                        const auto& process_info = process_infos[0];
                        const auto handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_info.pid);
                        TerminateProcess(handle, 0);
                        process::launch(config->process_launch_parameters);
                    }
                    else
                    {
                        dlog.println("hung acc {}", timer0.peek());
                    }
                }
                else
                {
                    IsHung = false;
                }
            }
            else
            {
                dlog.println("not find the window from {}, so it will never hung", resolver.file_full_name);
            }
        }
    }
    timer timer0;
    bool IsHung = false;
    WatchDogConfig* config = &get_config();
};




export void process_message(Log& log, const std::vector<char>& message)
{
   
    if (message.empty())
    {
        log.println("receive empty message");
        return;
    }
    std::string ms(message.data(), message.back() == '\0' ? message.size() - 1 : message.size());
    if (ms == "launch")
    {
        log.println("receive the cmd launch, launch {}", g_config.process_launch_parameters.app_path);
        process::launch(g_config.process_launch_parameters);
    }else if(ms == "relaunch")
    {
        log.println("receive cmd relaunch, relaunch {}", g_config.process_launch_parameters.app_path);
        process::kill(path_resolver(g_config.process_launch_parameters.app_path).file_full_name);
        process::launch(g_config.process_launch_parameters);
    }
    else if (ms.starts_with("launch "))
    {
        log.println("receive {}", ms);

        const std::string_view path = std::string_view(ms).substr(7);
        if (std::filesystem::exists(path))
        {
            process::launch({ {path.data(),path.size() } });
        }
        //process::process_launch_parameters parameters;
    }
    else if (ms.starts_with("relaunch "))
    {
        log.println("receive {}", ms);
        const std::string_view path = std::string_view(ms).substr(9);
        if (std::filesystem::exists(path))
        {
            process::kill(path_resolver(std::filesystem::absolute(path).string()).file_full_name);
            process::launch({ {path.data(),path.size() } });
        }
    }
    else if (ms == "shutdown")
    {
        log.println("receive shutdown command, shutdown {}", g_config.process_launch_parameters.app_path);
        process::kill(path_resolver(g_config.process_launch_parameters.app_path).file_full_name);
    }
    else if (ms == "force-shutdown")
    {
        log.println("receive shutdown command, shutdown {}", g_config.process_launch_parameters.app_path);
        process::force_kill(path_resolver(g_config.process_launch_parameters.app_path).file_full_name);
    }
    else if (ms.starts_with("shutdown "))
    {
        log.println("receive {}", ms);
	    const std::string path = ms.substr(9);
        process::kill(path_resolver(path).file_full_name);

        //process::process_launch_parameters parameters;
    }
    else if (ms.starts_with("force-shutdown "))
    {
        log.println("receive {}", ms);
        const std::string path = ms.substr(9);
        process::force_kill(path_resolver(path).file_full_name);

        //process::process_launch_parameters parameters;
    }
    else if (ms == "protect-enable")
    {
        log.println("receive cmd protect-enable, enable the watchdog");
        g_config.enable_watchdog = true;
    }
    else if (ms == "protect-disable")
    {
        log.println("receive cmd protect-disable, enable the watchdog");
        g_config.enable_watchdog = false;
    }
    else
    {
        log.println("receive \'{}\', unjustified command, will be ignored", ms);
    }
}

//bool(*IsHungAppWindow)(HWND);
class FuncRegister
{
public:
    inline static HMODULE m_hUser32 = nullptr;
    inline static FARPROC m_IsHungNT = nullptr;
    FuncRegister()
    {
        if (!m_hUser32)
        {
            m_hUser32 = GetModuleHandleA("user32.dll");
        }
        if (!m_hUser32)
        {
            m_hUser32 = LoadLibraryA("user32.dll");
        }

        if (m_hUser32)
        {
            m_IsHungNT = GetProcAddress(m_hUser32, "IsHungAppWindow");
        }
        else
        {
            int err = GetLastError();
            println("ErrCode: {}", err);
        }
    }

    static bool IsHungAppWindow(HWND hWnd)
    {
        static bool(*func)(HWND) = reinterpret_cast<bool(*)(HWND)> (m_IsHungNT);

        if (func)
            return func(hWnd);
        else
            throw "IsHungAppWindow is Not valid";
        return false;
    }
};
FuncRegister g_register;

