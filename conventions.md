# C++ Naming Conventions

## Files
- Header files: `.hpp`
- Source files: `.cpp`
- Example: `my_class.hpp` / `my_class.cpp`

## Classes & Structs
- PascalCase: `MyClass`, `PlayerController`

## Functions & Methods
- snake_case: `do_something()`, `get_value()`

## Variables
- Local: `snake_case` → `my_var`
- Local static: prefix `s_` → `s_static_var`
- Member (public): `snake_case` → `my_var`
- Member (protected/private): suffix `_` → `my_var_`
- Member static (public): prefix `s_` → `s_static_var`
- Member static (protected/private): prefix `s_`, suffix `_` → `s_static_var_`
- Constants: `UPPER_SNAKE_CASE` → `MAX_SIZE`
- Constexpr: prefix `K_` → `K_MAX_SIZE`
- Static constexpr: follow constexpr

## Namespaces
- `snake_case`: `my_namespace`

## Enums
- Enum type: PascalCase → `Direction`
- Enum values: `UPPER_SNAKE_CASE` → `NORTH`, `SOUTH`

## Templates
- Type parameters: single uppercase or PascalCase → `T`, `TValue`

## Macros
- `UPPER_SNAKE_CASE`: `MY_MACRO`