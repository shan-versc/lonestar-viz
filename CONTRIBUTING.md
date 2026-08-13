# Contributing

## Prerequisites

- CMake ≥ 3.24
- C++20 compiler: GCC 12+, Clang 15+, or MSVC 19.35+
- OpenGL 4.1 capable GPU driver
- Linux: `libgl-dev`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxi-dev`

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/aarf_cartographer
```

## Branch conventions

| Branch prefix       | Purpose                        |
|---------------------|--------------------------------|
| `feature/*`         | New features                   |
| `fix/*`             | Bug fixes                      |
| `perf/*`            | Performance improvements       |
| `docs/*`            | Documentation only             |

## Commit style

```
type(scope): short imperative description

Optional body.
```

Types: `feat`, `fix`, `perf`, `refactor`, `docs`, `build`, `ci`, `chore`.

## Code style

- C++20, no exceptions in hot paths
- `snake_case` for variables/functions, `PascalCase` for types
- 4-space indent, 100-col limit
- All public API members documented with `///`
- No raw `new`/`delete` — use RAII, `unique_ptr`, or EnTT storage
