include(FetchContent)

set(YLT_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(YLT_ENABLE_IO_URING ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        yalantinglibs
        GIT_REPOSITORY https://github.com/alibaba/yalantinglibs.git
        GIT_TAG main
        GIT_SHALLOW 1
)

FetchContent_MakeAvailable(yalantinglibs)
