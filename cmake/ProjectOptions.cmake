# Centralized build options for long-lived maintainability.
#- Simply project options, warning levels, and sanitizer settings.
# Design goals:
# - Keep target configuration consistent across the repo.
# - Avoid sprinkling warning/sanitizer flags throughout CMakeLists.
# - Make it easy to evolve policies without touching many files.

include_guard(GLOBAL)

function(core_engine_apply_project_options target)
  # Compile settings that are safe and non-controversial.
  target_compile_definitions(${target}
    PRIVATE
      $<$<CONFIG:Debug>:CORE_ENGINE_DEBUG_BUILD>
  )

  if(MSVC)
    target_compile_options(${target} PRIVATE /permissive- /Zc:__cplusplus)
  endif()
endfunction()

function(core_engine_apply_warnings target warnings_as_errors)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(warnings_as_errors)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(warnings_as_errors)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

function(core_engine_apply_sanitizers target enable)
  if(NOT enable)
    return()
  endif()

  # Sanitizers are typically not usable on MSVC the same way as Clang/GCC.
  if(MSVC)
    message(WARNING "Sanitizers requested but MSVC sanitizer flags are not configured. Consider using clang-cl.")
    return()
  endif()

  # Keep the set small and predictable. Expand only with intent.
  target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
endfunction()
