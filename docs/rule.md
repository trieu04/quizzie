# Project Rules & Coding Standards

## 1. Project Structure

The project is organized into the following directories:

- **`server/`**: Contains the server-side source code (`src/`) and headers (`include/`).
- **`client/`**: Contains the client-side source code (`src/`) and headers (`include/`).
- **`shared/`**: Contains code shared between client and server (e.g., `protocol.h`, `cJSON`).
- **`docs/`**: Documentation files (SRS, Specifications, Rules).
- **`tests/`**: Python-based integration tests.
- **`bin/`**: Compiled binaries (generated).
- **`obj/`**: Intermediate object files (generated).

## 2. Shared Code Conventions

- **Location**: All shared header files MUST be placed in `shared/include/`.
- **Usage**: unique code shared between Client and Server (e.g., Protocol definitions, common data structures) belongs here.
- **Include Path**: The `Makefile` configures the include path so that you can include these files directly.
    - **Correct**: `#include "protocol.h"`
    - **Incorrect**: `#include "../../shared/include/protocol.h"`

## 3. Technology Stack

- **Language**: C (C99 Standard / GNU Extensions).
- **GUI Library**: GTK+ 3.0 (for Client).
- **Network**: POSIX Sockets (TCP/IP).
- **Concurrency**: `poll()` for Server I/O multiplexing.
- **Build System**: GNU Make.
- **Testing**: Python 3 (standard `unittest` or custom runner).

## 4. Naming Conventions

Consistency is key. Follow these conventions:

- **Variables & Functions**: `snake_case`
    - Example: `client_count`, `handle_login()`, `server_start()`
- **Constants & Macros**: `SCREAMING_SNAKE_CASE`
    - Example: `MAX_CLIENTS`, `DEFAULT_PORT`
- **Structs & Typedefs**: `CamelCase`
    - Example: `ClientState`, `PacketHeader`
- **File Names**: `snake_case.c`, `snake_case.h`
    - Example: `ui_login.c`, `protocol.h`

## 5. Coding Style

- **Indentation**: 4 spaces. No tabs.
- **Braces**: K&R Style (opening brace on the same line).
    ```c
    if (condition) {
        // code
    } else {
        // code
    }
    ```

    }
    ```

## 6. Code Quality & Philosophy

- **Concise**: Write simple, direct code. Avoid over-engineering.
- **Readable**: Code must be self-explanatory. Use meaningful names. If a comment is needed to explain *what* the code does, clarity the code first.
- **Maintainable**:
    - **DRY (Don't Repeat Yourself)**: Extract common logic into shared functions.
    - **Single Responsibility**: Each function should do one thing well.

## 7. Development Workflow

- **Compilation**:
    - Run `make` to build both client and server.
    - Run `make clean` before a fresh build if headers change.
- **Testing**:
    - Build first: `make`
    - Run auth tests: `make test` or `python3 tests/runner.py`
