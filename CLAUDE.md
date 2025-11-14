# CLAUDE.md - AI Assistant Guide for ImGui Terminal App

This document provides comprehensive guidance for AI assistants (like Claude) working on this codebase. It contains essential context, conventions, and workflows to help you quickly understand and effectively contribute to this project.

## Table of Contents
- [Project Overview](#project-overview)
- [Repository Structure](#repository-structure)
- [Build System](#build-system)
- [Code Organization](#code-organization)
- [Development Workflows](#development-workflows)
- [Key Technologies](#key-technologies)
- [Code Conventions](#code-conventions)
- [Testing and Debugging](#testing-and-debugging)
- [Common Tasks](#common-tasks)
- [Platform-Specific Notes](#platform-specific-notes)

---

## Project Overview

**Project Name:** ImGui Terminal Command Runner
**Version:** Based on Dear ImGui v1.92.5 WIP (docking branch)
**License:** MIT
**Repository:** harsha-deep/imgui-app
**Main Development File:** src/linux/application.cpp:1-505

### What This Application Does
This is a cross-platform GUI application built with Dear ImGui that implements a **live terminal command runner** with real-time output streaming. It features:
- Real-time command execution with live output streaming
- Command history with arrow key navigation (up to 50 commands)
- Process lifecycle management (start/stop/kill)
- Non-blocking I/O with thread-safe operations
- Interactive command warnings
- Menu-driven command examples
- Configurable UI (auto-scroll, timestamps, output height)

### Technology Stack
- **UI Framework:** Dear ImGui v1.92.5 (immediate mode GUI)
- **Windowing:** GLFW 3.x (cross-platform)
- **Rendering:** OpenGL 3.0+ (Linux/macOS), DirectX 11 (Windows)
- **Language:** C++17
- **Build System:** Makefile (cross-platform)
- **Threading:** std::thread, std::mutex, std::atomic

---

## Repository Structure

```
imgui-app/
├── src/                    # Application source code
│   ├── linux/             # Linux-specific implementation (PRIMARY)
│   │   ├── main.cpp       # Entry point (GLFW + OpenGL3)
│   │   ├── application.cpp # Terminal runner implementation (505 lines)
│   │   └── application.h   # Application interface
│   ├── win32/             # Windows DirectX11 implementation
│   │   └── main.cpp       # Windows entry point (demo only)
│   └── libs/              # Embedded library headers
│       ├── emscripten/    # WebAssembly support
│       ├── glfw/          # GLFW headers
│       └── usynergy/      # Synergy client library
│
├── lib/                   # ImGui core library (~3.9 MB)
│   └── imgui/            # Dear ImGui source files
│       ├── imgui.cpp/h   # Core ImGui
│       ├── imgui_demo.cpp # Feature showcase
│       ├── imgui_draw.cpp # Rendering
│       ├── imgui_tables.cpp # Table widgets
│       ├── imgui_widgets.cpp # UI components
│       └── im*.h         # Internal headers and STB libs
│
├── backends/              # Platform & renderer backends (~1.1 MB)
│   ├── imgui_impl_glfw.cpp/h      # GLFW backend (used)
│   ├── imgui_impl_opengl3.cpp/h   # OpenGL backend (used)
│   ├── imgui_impl_win32.cpp/h     # Win32 backend
│   ├── imgui_impl_dx*.cpp/h       # DirectX backends
│   └── [other backends...]        # SDL, Vulkan, Metal, etc.
│
├── misc/                  # Utilities and helpers (~586 KB)
│   ├── fonts/            # Sample TrueType fonts
│   ├── freetype/         # FreeType integration
│   ├── debuggers/        # VS/GDB/LLDB visualizers
│   ├── cpp/              # STL integration (imgui_stdlib)
│   └── single_file/      # Unity build support
│
├── docs/                  # Dear ImGui documentation (~813 KB)
│   ├── README.md         # Main ImGui docs
│   ├── BACKENDS.md       # Backend implementation guide
│   ├── FAQ.md            # Common questions
│   ├── FONTS.md          # Font configuration
│   ├── EXAMPLES.md       # Example applications
│   ├── CONTRIBUTING.md   # Contribution guidelines
│   └── CHANGELOG.txt     # Version history
│
├── Makefile              # Cross-platform build configuration
├── README.md             # Project README
├── LICENSE.txt           # MIT License
└── .gitignore           # Git ignore rules

```

### Important Files for AI Assistants

| File | Purpose | When to Edit |
|------|---------|-------------|
| `src/linux/application.cpp` | Main application logic | Adding features, fixing bugs |
| `src/linux/main.cpp` | Entry point & initialization | Window setup, ImGui config |
| `Makefile` | Build configuration | Adding sources, changing flags |
| `.gitignore` | Git exclusions | Adding new build artifacts |
| `src/linux/application.h` | Application interface | Exposing new functions |

---

## Build System

### Makefile Overview
The project uses a **simple, cross-platform Makefile** (no CMake, no npm, no complex tools).

#### Build Directories
- **Source:** `src/linux/` (configurable via `SRC_DIR`)
- **Build artifacts:** `build/` (object files)
- **Executables:** `bin/` (output binary: `bin/app`)

#### Common Build Commands
```bash
make          # Build the application
make clean    # Remove build/ and bin/
make run      # Build and execute
make setup    # Create build directories
```

### Compiler Configuration
- **Compiler:** g++
- **Standard:** C++17 (`-std=c++17`)
- **Flags:** `-g -Wall -Wformat`
- **Include paths:**
  - `lib/imgui/`
  - `backends/`
  - `src/linux/`

### Platform-Specific Libraries

**Linux:**
```makefile
LIBS = -lGL -lX11 `pkg-config --static --libs glfw3`
CXXFLAGS += `pkg-config --cflags glfw3`
```

**macOS:**
```makefile
LIBS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lglfw
CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
```

**Windows (MinGW):**
```makefile
LIBS = -lglfw3 -lgdi32 -lopengl32 -limm32
```

### Adding New Source Files
Edit the `SOURCES` variable in Makefile:
```makefile
SOURCES = \
    $(SRC_DIR)/main.cpp \
    $(SRC_DIR)/application.cpp \
    $(SRC_DIR)/new_file.cpp \  # Add here
    $(IMGUI_DIR)/imgui.cpp \
    # ... rest of files
```

---

## Code Organization

### Namespace Convention
All application code lives in the `myApp` namespace:
```cpp
namespace myApp
{
    void renderUI();  // Main render function
    // ... application code
}
```

### File Organization Pattern
```cpp
// 1. Standard includes
#include "application.h"
#include "imgui.h"

// 2. STL includes
#include <thread>
#include <mutex>
#include <string>

// 3. Platform-specific includes
#include <unistd.h>      // Linux
#include <sys/wait.h>

// 4. Namespace
namespace myApp
{
    // 5. Static variables (file-scoped)
    static std::deque<std::string> g_command_history;
    static std::mutex g_mutex;

    // 6. Helper functions (static)
    static std::string get_timestamp() { ... }

    // 7. Public functions
    void renderUI() { ... }
}
```

### Variable Naming Conventions
- **Global/static variables:** `g_` prefix (e.g., `g_process`, `g_mutex`)
- **Constants:** `UPPER_SNAKE_CASE` (e.g., `MAX_HISTORY`)
- **Local variables:** `snake_case` (e.g., `command`, `output_height`)
- **Functions:** `snake_case` (e.g., `add_to_history()`, `kill_process()`)
- **Structs/Classes:** `PascalCase` (e.g., `ProcessHandle`, `RunnerCleaner`)

### Code Style
- **Indentation:** 4 spaces (no tabs)
- **Braces:** Opening brace on same line
- **Line length:** No strict limit, but prefer readability
- **Comments:** Use `//` for single-line, descriptive section headers

Example:
```cpp
static void add_to_history(const std::string &cmd)
{
    if (cmd.empty())
        return;

    // Don't add duplicates of the last command
    if (!g_command_history.empty() && g_command_history.back() == cmd)
        return;

    g_command_history.push_back(cmd);
}
```

---

## Development Workflows

### Git Workflow

**Current Branch:** `claude/claude-md-mhyecl71esjx0tml-0197QVfiWod886dkQwdnqueK`

#### Standard Git Operations
```bash
# 1. Make changes to code
# 2. Build and test
make clean && make run

# 3. Commit changes
git add <files>
git commit -m "Descriptive message"

# 4. Push to branch
git push -u origin claude/claude-md-mhyecl71esjx0tml-0197QVfiWod886dkQwdnqueK
```

#### Commit Message Style
Based on recent history:
- Use imperative mood: "Add feature" not "Added feature"
- Be concise but descriptive
- Examples:
  - "Add command history navigation with arrow keys"
  - "Fix segmentation fault in process cleanup"
  - "Update window size and DPI scaling"
  - "Folder restructure for better organization"

### Development Cycle
1. **Plan:** Understand the requirement
2. **Locate:** Find relevant code in `src/linux/application.cpp`
3. **Modify:** Make changes following conventions
4. **Build:** `make clean && make`
5. **Test:** `make run` and verify functionality
6. **Commit:** Create descriptive commit
7. **Push:** Push to feature branch

### Recent Development Patterns
Based on git history:
- Iterative improvements (segfault fixes, window sizing)
- Code reorganization (folder restructuring)
- Documentation updates (README links)
- Cleanup (removing .vscode directory)

---

## Key Technologies

### Dear ImGui Fundamentals

**Immediate Mode GUI Paradigm:**
- No retained state in widgets
- UI code runs every frame
- Simple, imperative API

**Key Features Enabled:**
```cpp
// In main.cpp initialization
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Keyboard controls
io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Gamepad support
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Docking windows
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Multi-viewport
```

**Common ImGui Patterns Used:**
```cpp
// Window creation
ImGui::Begin("Window Title");
// ... content
ImGui::End();

// Input fields
static char buffer[256];
if (ImGui::InputText("Label", buffer, sizeof(buffer))) {
    // Handle input
}

// Buttons
if (ImGui::Button("Click Me")) {
    // Handle click
}

// Text output
ImGui::TextUnformatted(text.c_str());
```

### Threading and Synchronization

**Pattern Used in Application:**
```cpp
// Shared state
static std::mutex g_mutex;
static std::string g_output;
static std::atomic<bool> g_running(false);

// Thread-safe write
void worker_thread() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_output += "New data\n";
}

// Thread-safe read (in main/render thread)
void renderUI() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ImGui::TextUnformatted(g_output.c_str());
}
```

**RAII Cleanup Pattern:**
```cpp
struct RunnerCleaner {
    ~RunnerCleaner() {
        // Cleanup on scope exit
        if (g_worker.joinable())
            g_worker.join();
    }
};
static RunnerCleaner s_cleaner;
```

### Linux Process Management

**Key APIs Used:**
- `pipe()` - Create pipe for IPC
- `fork()` - Create child process
- `execl()` - Execute command
- `setpgid()` - Set process group
- `kill()` - Send signals
- `waitpid()` - Wait for process termination
- `fcntl()` - Non-blocking I/O

**Process Execution Pattern:**
```cpp
int pipefd[2];
pipe(pipefd);

pid_t pid = fork();
if (pid == 0) {
    // Child process
    setpgid(0, 0);  // New process group
    dup2(pipefd[1], STDOUT_FILENO);
    execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
} else {
    // Parent process
    g_process.pid = pid;
    g_process.pipe = fdopen(pipefd[0], "r");
    // Read output...
}
```

---

## Code Conventions

### Error Handling
- Use descriptive error messages
- Prefix errors with `[ERROR]`
- Always clean up resources on error
```cpp
if (pipe(pipefd) == -1) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_output += "[ERROR] Failed to create pipe\n";
    g_running = false;
    return;
}
```

### Thread Safety
- **Always** lock `g_mutex` when accessing shared state
- Use `std::atomic` for simple flags
- Prefer `std::lock_guard` over manual lock/unlock
- Join threads before program exit

### Memory Management
- Prefer stack allocation
- Use RAII for resource cleanup
- Close file descriptors and pipes
- Join threads properly
```cpp
if (g_process.pipe) {
    fclose(g_process.pipe);
    g_process.pipe = nullptr;
}
```

### Input Validation
- Check for empty commands
- Warn about interactive commands
- Validate buffer sizes
```cpp
if (command.empty())
    return;

// Check for interactive commands
if (command.find("vim") != std::string::npos) {
    g_output += "[WARNING] Interactive commands may not work properly\n";
}
```

---

## Testing and Debugging

### Manual Testing Workflow
1. **Build:** `make clean && make`
2. **Run:** `make run` or `./bin/app`
3. **Test commands:**
   - Simple: `ls`, `pwd`, `uname`
   - Long-running: `ping localhost -c 5`
   - Output-heavy: `find /usr -name "*.so"`
4. **Test features:**
   - Command history (Up/Down arrows)
   - Stop button during execution
   - Auto-scroll toggle
   - Timestamp toggle

### Debugging Tools Available

**GDB/LLDB:**
```bash
gdb ./bin/app
# Debugger visualizers in misc/debuggers/imgui.gdb
```

**ImGui Metrics:**
```cpp
// Add to renderUI()
ImGui::ShowMetricsWindow();
```

**Build with debug symbols:**
Already enabled with `-g` flag in Makefile.

### Common Issues and Solutions

**Segmentation Fault:**
- Recent fix: Process cleanup on application exit
- Location: src/linux/application.cpp (RunnerCleaner)
- Solution: Always join threads before exit

**Window Size Issues:**
- Recent fix: DPI scaling and window size adjustments
- Location: src/linux/main.cpp (window creation)

**Interactive Commands:**
- Issue: Commands like vim, nano don't work
- Reason: No TTY allocation
- Solution: Show warning to user

---

## Common Tasks

### Adding a New Menu Command
```cpp
// In renderUI(), within the menu bar
if (ImGui::MenuItem("New Command"))
{
    strcpy(g_command_buffer, "your command here");
}
```

### Adding a New UI Control
```cpp
// Static variable for state
static bool new_option = false;

// In renderUI()
if (ImGui::Checkbox("New Option", &new_option)) {
    // Handle toggle
}
```

### Modifying Command History Size
```cpp
// In application.cpp
static constexpr size_t MAX_HISTORY = 100;  // Change from 50
```

### Adding Platform-Specific Code
```cpp
#ifdef __linux__
    // Linux-specific code
#elif __APPLE__
    // macOS-specific code
#elif _WIN32
    // Windows-specific code
#endif
```

### Changing Window Defaults
```cpp
// In main.cpp
GLFWwindow* window = glfwCreateWindow(
    1920,  // width
    1080,  // height
    "New Title",
    nullptr,
    nullptr
);
```

---

## Platform-Specific Notes

### Linux (Primary Platform)
- **Status:** Fully implemented
- **Backend:** GLFW + OpenGL3
- **Features:** Full terminal command runner
- **Dependencies:**
  ```bash
  sudo apt install libglfw3-dev libgl1-mesa-dev libx11-dev
  ```
- **Build:** `make`

### macOS
- **Status:** Should work (uses Linux source)
- **Backend:** GLFW + OpenGL3
- **Note:** OpenGL 3.2 Core Profile required
- **Dependencies:**
  ```bash
  brew install glfw
  ```
- **Build:** `make` (auto-detects macOS)

### Windows
- **Status:** Basic demo only
- **Backend:** Win32 + DirectX11
- **Implementation:** src/win32/main.cpp
- **Note:** Does not include terminal runner
- **Build:** MinGW or Visual Studio

### Cross-Platform Considerations
- Keep Linux-specific code in `src/linux/`
- Use `#ifdef` for platform detection
- Test on target platform before committing
- Process management is Linux-specific (fork, exec)

---

## Additional Resources

### Dear ImGui Documentation
- **Main docs:** docs/README.md
- **Backend guide:** docs/BACKENDS.md
- **FAQ:** docs/FAQ.md
- **Examples:** docs/EXAMPLES.md
- **Demo code:** lib/imgui/imgui_demo.cpp

### External Links
- [Dear ImGui GitHub](https://github.com/ocornut/imgui)
- [GLFW Documentation](https://www.glfw.org/docs/latest/)
- [OpenGL Reference](https://www.khronos.org/opengl/wiki/)

### Code References
When referencing code, use format: `file:line`
- Main app logic: src/linux/application.cpp:1-505
- Entry point: src/linux/main.cpp:1-209
- Interface: src/linux/application.h:1-7

---

## Quick Start for AI Assistants

When working on this codebase:

1. **Understand the context:** Read this CLAUDE.md file
2. **Locate the code:** Most work happens in `src/linux/application.cpp`
3. **Check the build:** Run `make` to ensure it compiles
4. **Follow conventions:** Use the patterns described above
5. **Test your changes:** Use `make run`
6. **Commit properly:** Use descriptive commit messages
7. **Push carefully:** Push to the correct feature branch

### Before Making Changes
- [ ] Understand what the code currently does
- [ ] Identify which file(s) need modification
- [ ] Check for platform-specific implications
- [ ] Consider thread safety if modifying shared state

### After Making Changes
- [ ] Code compiles without warnings
- [ ] Application runs without crashes
- [ ] Feature works as expected
- [ ] No thread safety issues introduced
- [ ] Commit message is descriptive
- [ ] Changes pushed to correct branch

---

**Last Updated:** 2025-11-14
**ImGui Version:** v1.92.5 WIP (docking branch)
**Project Status:** Active Development
**Primary Contact:** See repository for maintainer info
