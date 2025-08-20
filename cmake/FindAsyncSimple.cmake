include(FetchContent)

# Options you used in your manual build
set(ASYNC_SIMPLE_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ASYNC_SIMPLE_BUILD_DEMO_EXAMPLE OFF CACHE BOOL "" FORCE)
set(ASYNC_SIMPLE_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
set(ASYNC_SIMPLE_UTHREAD OFF CACHE BOOL "" FORCE)

# Fetch shallow clone of main branch
FetchContent_Declare(
        async_simple
        GIT_REPOSITORY https://github.com/alibaba/async_simple.git
        GIT_TAG 1.4
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""
)

# Add the dependency; it provides targets we can re-export
FetchContent_MakeAvailable(async_simple)
