module;

#include <ranges>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
//#include <format>
#include <fstream>
#include <filesystem>
#include <queue>
#include <set>
#include <Windows.h>
#include <TlHelp32.h>
#include "code_trans.h"
#include "print.h"
#include "worker.h"
#include "thread_pool.h"
#include "json.hpp"
#include "udp_receiver.h"




export module utils;

export {


    

    /**
     * \brief search the process with the input name by process snapshot
     * \param names used to filter the corresponding process
     * \return the mapping of the program and its name
     */
    std::map<std::string, std::vector<PROCESSENTRY32W>> findProcessEntry(const std::set<std::string>& names)
    {
        const auto hProcessSnapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        if (hProcessSnapShot == INVALID_HANDLE_VALUE)
        {
            println("CreateToolhelp32Snapshot Failed!");
        }
        PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
        std::map<std::string, std::vector<PROCESSENTRY32>> ProcessEntryMap;
        if (Process32First(hProcessSnapShot, &pe32))
        {
            do
            {
                std::wstring_view wsv{ pe32.szExeFile };
                if (std::set<std::string>::const_iterator it;
                    (it = names.find(WideCharToAnsi(std::wstring(wsv.substr(0, wsv.find_first_of('.'))).c_str()))) != names.cend())
                {
                    ProcessEntryMap[*it].emplace_back(pe32);
                }
            } while (Process32Next(hProcessSnapShot, &pe32));
        }
        return ProcessEntryMap;
    }


    std::map<DWORD, std::vector<HWND>> pid_hwnd_map;
    BOOL EnumWindowsCallBack(HWND hwnd, LPARAM lparam)
    {
        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid_hwnd_map.contains(pid))
        {
            pid_hwnd_map[pid].emplace_back(hwnd);
        }
        return true;
    }

    struct WindowInfo
    {
        HWND hwnd = nullptr;
        std::string name;
        bool hung = false;
    };

    struct ProcessInfo
    {
        DWORD pid;
        std::string name;
        std::vector<WindowInfo> window_infos;
    };
    bool get_is_hung(const ProcessInfo& process_info)
    {
        bool hung = false;
        for (auto& window_info : process_info.window_infos)
        {
            hung |= window_info.hung;
        }
        return hung;
    }
    bool get_has_window(const ProcessInfo& process_info)
    {
        return !process_info.window_infos.empty();
    }

    std::map<std::string, std::vector<ProcessInfo>> get_process_info(std::vector<std::string> process_name)
    {
        std::map<DWORD, ProcessInfo> process_info_map;

        auto find_result = findProcessEntry({ process_name.begin(), process_name.end() });

        for (const auto& pair : find_result)
        {
            if (const auto& [proc_name, process_entry_vec] = pair; !process_entry_vec.empty())
            {

                for (const auto& process_entry : process_entry_vec)
                {
                    auto pid = process_entry.th32ProcessID;
                    auto& process_info = process_info_map[pid];
                    process_info_map.insert_or_assign(pid, ProcessInfo{
                        pid,proc_name,{}
                        });
                }
            }
        }

        pid_hwnd_map.clear();
        for (auto name : find_result | std::ranges::views::values | std::ranges::views::join)
        {
            pid_hwnd_map.emplace(name.th32ProcessID, std::vector<HWND>{});
        }
        EnumWindows(EnumWindowsCallBack, 123456);


        std::vector<std::pair<std::string, HWND>> name_hwnds; // 窗口名及其句柄
        for (auto& [pid, hwnds] : pid_hwnd_map/* | std::ranges::views::values | std::ranges::views::join*/)
        {
            for (auto hwnd : hwnds)
            {
                std::array<char, 256> buf;
                GetWindowTextA(hwnd, buf.data(), 256);
                name_hwnds.emplace_back(buf.data(), hwnd);
                bool hung = IsHungAppWindow(hwnd);
                process_info_map[pid].window_infos.emplace_back(hwnd, buf.data(), hung);
            }

        }
        std::map<std::string, std::vector<ProcessInfo>> ret;
        for (auto& process_info : process_info_map | std::ranges::views::values)
        {
            ret[process_info.name].emplace_back(process_info);
        }

        return ret;
    }
    std::vector<ProcessInfo> get_process_info(const std::string& process_name)
    {
        return get_process_info(std::vector{ process_name })[process_name];
    }

    struct Log
    {
        static std::deque<std::string>& message_deque()
        {
            static std::deque<std::string> msg_deq;
            return msg_deq;
        }
        static std::vector<std::function<void(std::string_view)>>& slots()
        {
            static std::vector<std::function<void(std::string_view)>> slot_set;
            return slot_set;
        }

        std::shared_ptr<udp_sender> sender = nullptr;
        Log() = default;
        Log(const Log&) = default;
        Log(Log&&) = default;
        Log& operator=(const Log&) = default;
        Log& operator=(Log&&) = default;
        explicit Log(const std::shared_ptr<udp_sender>& sender)
            :sender(sender)
        {

        }
        ~Log() = default;

        void sync_out(std::string_view s, bool b = false) const
        {
            if (sender)
                sender->send(std::vector<char>{ s.cbegin(), s.cend() });
            ::sync_out(s, b);
            message_deque().emplace_front(s);
            while(message_deque().size() > 100)
            {
                message_deque().pop_back();
            }
            for (auto& f : slots())
                f(s);
        }

        template <class... Types> requires (sizeof...(Types) > 0)
            _NODISCARD void print(const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
            sync_out(_STD vformat(Fmt._Str, _STD make_format_args(Args...)));
        }
        template <int = 0>
        void print(std::string_view s) const
        {
            sync_out(s);
        }
        template <class... Types> requires (sizeof...(Types) > 0)
            _NODISCARD void println(const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
            sync_out(_STD vformat(Fmt._Str, _STD make_format_args(Args...)), true);
        }
        template <int = 0>
        void println(std::string_view s) const
        {
            sync_out(s, true);
        }
    } dlog;
    

    class timer
    {
        using clock = std::chrono::high_resolution_clock;

    public:
        timer() { reset(); }

        void reset()
        {
            start_tp = clock::now();
        }

        auto peek()
        {
            auto peek_tp = clock::now() - start_tp;
            return std::chrono::duration<double>(peek_tp);
        }
        auto seek()
        {
            auto now = clock::now();
            auto peek_tp = now - std::exchange(start_tp, now);
            return std::chrono::duration<double>(peek_tp);
        }
    private:
        clock::time_point start_tp = clock::now();
    };

    struct path_resolver
    {
        std::string file_name;
        std::string file_full_name;
        std::string file_dir;
        std::string file_path;

        path_resolver(std::string_view path)
        {
            file_path = path;
            if (size_t index;
                (index = path.find_last_of('\\')) != std::string::npos
                || (index = path.find_last_of('/')) != std::string::npos
                )
            {
                file_dir = path.substr(0, index);
                file_full_name = path.substr(index + 1, path.size() - index + 1);
                const auto dot_index = file_full_name.find_last_of('.');
                if (dot_index != std::string::npos)
                    file_name = file_full_name.substr(0, dot_index);
                else
                    file_name = file_full_name;
            }
        }
    };

    template<typename config_t> requires requires(config_t config, nlohmann::json json) { config = json; json = config; }
    void deserialize_from_file(config_t& config, std::string_view config_path)
    {
        int run = true;
        while (run)
        {
            try
            {
                if (!std::filesystem::exists(config_path))
                {
                    std::ofstream ofstream(config_path.data());

                    if (ofstream.is_open())
                    {
                        ofstream << nlohmann::json(config).dump(4);
                    }
                }
                std::ifstream ifstream(config_path.data());
                std::string config_json_string;
                if (ifstream.is_open())
                {
                    ifstream.seekg(0, std::ifstream::end);
                    auto size = ifstream.tellg();
                    ifstream.seekg(0, std::ifstream::beg);
                    config_json_string.resize(size);
                    ifstream.read(config_json_string.data(), size);
                }
                nlohmann::json json = nlohmann::json::parse(config_json_string);
                json.get_to(config);
                run = false;
            }
            catch (std::exception& exception)
            {
                auto text = std::format("Exception: {}\nWill using default setting:\n{}"
                    , exception.what()
                    , nlohmann::json(config).dump(4)
                );
                auto ret = MessageBoxA(nullptr, text.c_str(), "Read config error", MB_ICONERROR | MB_CANCELTRYCONTINUE);
                if (ret == IDCANCEL)
                    exit(-1);
                if (ret == IDRETRY)
                    continue;
                if (ret == IDCONTINUE)
                    run = false;
            }
        }
    }

    namespace process
    {
        struct process_launch_parameters
        {
            std::string app_path;
            std::string app_command_line;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(process_launch_parameters, app_path, app_command_line)
        };

        void launch(const process_launch_parameters& process_launch_parameters)
        {
            STARTUPINFOA startup_info;
            PROCESS_INFORMATION process_information;
            memset(&startup_info, 0, sizeof(startup_info));
            startup_info.cb = sizeof startup_info;
            CreateProcessA(
                process_launch_parameters.app_path.c_str()
                , nullptr
                , nullptr, nullptr, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL
                , &startup_info, &process_information
            );
        }
        void kill(std::string_view app_name)
        {
            system(std::format("TASKKILL /IM {}", app_name).c_str());
        }
        void force_kill(std::string_view app_name)
        {
            system(std::format("TASKKILL /F /IM {}", app_name).c_str());
        }
    }

}

