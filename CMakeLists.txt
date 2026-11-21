cmake_minimum_required(VERSION 3.10)
project(TerminalPad C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
        src/main.c
        src/undo/undo_stack.c
        src/text-engine/buffer/buffer.c
        src/text-engine/cursor/cursor.c
        src/search/search.c
        src/text-engine/selection/selection.c
        src/text-engine/clipboard/clipboard.c
        src/ui/renderer.c
        src/file-manager/file_manager.c
)

if(WIN32)
    list(APPEND SOURCES src/terminal/terminal_windows.c)
else()
    list(APPEND SOURCES src/terminal/terminal_linux.c)
endif()

add_executable(TerminalPad ${SOURCES})

target_include_directories(TerminalPad PRIVATE src)

if(WIN32)
    target_link_libraries(TerminalPad kernel32 user32)
endif()