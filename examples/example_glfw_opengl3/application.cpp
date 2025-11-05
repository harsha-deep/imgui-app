#include "application.h"
#include "imgui.h"

#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <deque>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>

// Platform-specific includes
#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#include <windows.h>
#include <signal.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace myApp
{
    // ----------------- Command History -----------------
    static std::deque<std::string> g_command_history;
    static constexpr size_t MAX_HISTORY = 50;
    static int g_history_index = -1;

    static void add_to_history(const std::string &cmd)
    {
        if (cmd.empty())
            return;

        // Don't add duplicates of the last command
        if (!g_command_history.empty() && g_command_history.back() == cmd)
            return;

        g_command_history.push_back(cmd);
        if (g_command_history.size() > MAX_HISTORY)
            g_command_history.pop_front();

        g_history_index = -1;
    }

    // ----------------- Process Management -----------------
    struct ProcessHandle
    {
        FILE *pipe = nullptr;
#ifdef _WIN32
        HANDLE process_handle = nullptr;
        DWORD process_id = 0;
#else
        pid_t pid = 0;
#endif
    };

    static ProcessHandle g_process;
    static std::thread g_worker;
    static std::mutex g_mutex;
    static std::string g_output;
    static std::atomic<bool> g_running(false);
    static std::atomic<bool> g_scroll_to_bottom(false);
    static std::atomic<bool> g_stop_requested(false);

    // Get current timestamp
    static std::string get_timestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    // Forcefully kill the running process
    static void kill_process()
    {
#ifdef _WIN32
        if (g_process.process_handle)
        {
            TerminateProcess(g_process.process_handle, 1);
            CloseHandle(g_process.process_handle);
            g_process.process_handle = nullptr;
        }
#else
        if (g_process.pid > 0)
        {
            // Kill the entire process group to catch child processes too
            kill(-g_process.pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // If still running, use SIGKILL
            kill(-g_process.pid, SIGKILL);
            waitpid(g_process.pid, nullptr, WNOHANG);
            g_process.pid = 0;
        }
#endif
    }

    static void run_command_thread(const std::string &command)
    {
        std::string cmd_with_redirect = command + " 2>&1";

#ifdef _WIN32
        // Windows: use _popen
        g_process.pipe = _popen(cmd_with_redirect.c_str(), "r");
#else
        // Unix/Linux: use popen with process group tracking
        // We need to fork manually to track PID for proper killing
        int pipefd[2];
        if (pipe(pipefd) == -1)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_output += "[ERROR] Failed to create pipe\n";
            g_running = false;
            g_scroll_to_bottom = true;
            return;
        }

        pid_t pid = fork();
        if (pid == -1)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_output += "[ERROR] Failed to fork process\n";
            close(pipefd[0]);
            close(pipefd[1]);
            g_running = false;
            g_scroll_to_bottom = true;
            return;
        }

        if (pid == 0)
        {
            // Child process
            close(pipefd[0]); // Close read end
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            // Create a new process group so we can kill child processes too
            setpgid(0, 0);

            // Execute the command via shell
            execl("/bin/sh", "sh", "-c", command.c_str(), (char *)nullptr);
            _exit(127); // If execl fails
        }

        // Parent process
        g_process.pid = pid;
        close(pipefd[1]); // Close write end
        g_process.pipe = fdopen(pipefd[0], "r");
#endif

        if (!g_process.pipe)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_output += "[ERROR] Failed to open command pipe\n";
            g_running = false;
            g_scroll_to_bottom = true;
            return;
        }

        // Set non-blocking mode for better responsiveness
#ifndef _WIN32
        int fd = fileno(g_process.pipe);
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

        constexpr size_t BUF_SIZE = 1024;
        char buffer[BUF_SIZE];

        // Read output line by line
        while (!g_stop_requested.load())
        {
            if (fgets(buffer, (int)BUF_SIZE, g_process.pipe) != nullptr)
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_output.append(buffer);
                g_scroll_to_bottom = true;
            }
            else
            {
                if (feof(g_process.pipe))
                {
                    break; // End of output
                }
                // No data available, sleep briefly to avoid CPU spinning
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Close pipe and get exit code
        int exit_code = 0;
        if (g_stop_requested.load())
        {
            kill_process();
            exit_code = -1;
        }

#ifdef _WIN32
        exit_code = _pclose(g_process.pipe);
#else
        if (g_process.pipe)
        {
            fclose(g_process.pipe);
            if (g_process.pid > 0)
            {
                int status;
                waitpid(g_process.pid, &status, 0);
                if (WIFEXITED(status))
                    exit_code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status))
                    exit_code = -1;
            }
        }
#endif

        g_process.pipe = nullptr;
        g_process.pid = 0;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_stop_requested.load())
            {
                g_output += "\n[STOPPED BY USER]\n";
            }
            else
            {
                g_output += "\n[Process exited with code: ";
                g_output += std::to_string(exit_code);
                g_output += "]\n";
            }
            g_scroll_to_bottom = true;
        }

        g_running = false;
        g_stop_requested = false;
    }

    // Check if command might need interactive input
    static bool is_interactive_command(const std::string &cmd)
    {
        return (cmd.find("sudo") != std::string::npos &&
                cmd.find("-S") == std::string::npos &&
                cmd.find("NOPASSWD") == std::string::npos) ||
               cmd.find("ssh") != std::string::npos ||
               cmd.find("passwd") != std::string::npos ||
               cmd.find("su ") != std::string::npos;
    }

    // Start a command in background
    static void start_command(const std::string &cmd, bool show_timestamp)
    {
        // Stop existing command if running
        if (g_running.load())
        {
            g_stop_requested = true;
            if (g_worker.joinable())
                g_worker.join();
            g_stop_requested = false;
        }

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_output.clear();

            if (show_timestamp)
                g_output += "[" + get_timestamp() + "] ";

            g_output += "$ " + cmd + "\n";

            // Warn about interactive commands
            if (is_interactive_command(cmd))
            {
                g_output += "[WARNING] This command may require interactive input (like passwords).\n";
                g_output += "[WARNING] Interactive input is not supported. The command may hang or fail.\n";
                g_output += "[TIP] For sudo, use: sudo -S (reads password from stdin) or configure NOPASSWD in sudoers.\n\n";
            }
        }

        add_to_history(cmd);
        g_running = true;
        g_scroll_to_bottom = true;
        g_stop_requested = false;
        g_worker = std::thread(run_command_thread, cmd);
    }

    // Stop the running command
    static void stop_command()
    {
        if (!g_running.load())
            return;

        g_stop_requested = true;

        // Give it a moment to stop gracefully
        for (int i = 0; i < 10 && g_running.load(); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Force kill if still running
        if (g_running.load())
            kill_process();

        if (g_worker.joinable())
            g_worker.join();

        g_stop_requested = false;
        g_running = false;
    }

    // Cleanup at shutdown
    static void cleanup_runner()
    {
        stop_command();
    }

    // ----------------- ImGui UI -----------------
    void renderUI()
    {
        static char command_buf[2048] = "ls -la";
        static bool auto_scroll = true;
        static bool show_timestamps = false;
        static float output_height = 400.0f;

        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("Command Runner", nullptr, ImGuiWindowFlags_MenuBar);

        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                ImGui::Checkbox("Auto-scroll", &auto_scroll);
                ImGui::Checkbox("Show Timestamps", &show_timestamps);
                ImGui::Separator();
                if (ImGui::MenuItem("Clear History"))
                {
                    g_command_history.clear();
                    g_history_index = -1;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Examples"))
            {
                if (ImGui::MenuItem("List files (ls -la)"))
                    strcpy(command_buf, "ls -la");
                if (ImGui::MenuItem("System info (uname -a)"))
                    strcpy(command_buf, "uname -a");
                if (ImGui::MenuItem("Disk usage (df -h)"))
                    strcpy(command_buf, "df -h");
                if (ImGui::MenuItem("Process list (ps aux)"))
                    strcpy(command_buf, "ps aux | head -20");
                if (ImGui::MenuItem("Ping test"))
                    strcpy(command_buf, "ping -c 5 8.8.8.8");
#ifndef _WIN32
                if (ImGui::MenuItem("Update packages (sudo apt update)"))
                    strcpy(command_buf, "sudo apt update");
#endif
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::TextWrapped("Enter a shell command and press Execute. Output streams in real-time below.");
        ImGui::Spacing();

        // Command input with history navigation
        ImGui::PushItemWidth(-120.0f);

        ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
        bool execute_pressed = ImGui::InputText("##command", command_buf,
                                                IM_ARRAYSIZE(command_buf), input_flags);

        // Command history navigation with Up/Down arrows
        if (ImGui::IsItemFocused() && !g_command_history.empty())
        {
            // Use ImGuiKey_ directly (newer ImGui API)
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                if (g_history_index < (int)g_command_history.size() - 1)
                {
                    g_history_index++;
                    strcpy(command_buf, g_command_history[g_command_history.size() - 1 - g_history_index].c_str());
                }
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                if (g_history_index > 0)
                {
                    g_history_index--;
                    strcpy(command_buf, g_command_history[g_command_history.size() - 1 - g_history_index].c_str());
                }
                else if (g_history_index == 0)
                {
                    g_history_index = -1;
                    command_buf[0] = '\0';
                }
            }
        }

        ImGui::PopItemWidth();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Use Up/Down arrows for command history");

        ImGui::SameLine();

        // Execute/Stop button
        bool can_execute = !g_running.load() && strlen(command_buf) > 0;
        if (can_execute)
        {
            if (ImGui::Button("Execute") || execute_pressed)
            {
                std::string cmd = command_buf;
                // Trim whitespace
                while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' '))
                    cmd.pop_back();
                if (!cmd.empty())
                    start_command(cmd, show_timestamps);
            }
        }
        else if (g_running.load())
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Stop"))
            {
                stop_command();
            }
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Execute");
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_output.clear();
        }

        // Output height slider
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        ImGui::SliderFloat("##height", &output_height, 100.0f, 800.0f, "Height: %.0f");

        ImGui::Separator();

        // Status indicator
        if (g_running.load())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "● Running");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "● Idle");
        }

        ImGui::SameLine();
        ImGui::Text("| Output:");

        ImGui::Spacing();

        // Output window with monospace font
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::BeginChild("##output_child", ImVec2(0, output_height), true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        // Copy output safely
        std::string out_copy;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            out_copy = g_output;
        }

        // Display output
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default monospace if available
        ImGui::TextUnformatted(out_copy.c_str());
        ImGui::PopFont();

        // Auto-scroll to bottom
        if (auto_scroll && (g_scroll_to_bottom.load() ||
                            ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f))
        {
            ImGui::SetScrollHereY(1.0f);
            g_scroll_to_bottom = false;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();

        // Info footer
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("History: %zu commands | Scroll: %.0f/%.0f",
                            g_command_history.size(),
                            ImGui::GetScrollY(),
                            ImGui::GetScrollMaxY());

        ImGui::End();
    }

    // RAII cleanup
    struct RunnerCleaner
    {
        ~RunnerCleaner() { cleanup_runner(); }
    };
    static RunnerCleaner s_runner_cleaner;

} // namespace myApp