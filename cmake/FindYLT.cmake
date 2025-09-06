include(FetchContent)

set(YLT_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(YLT_ENABLE_IO_URING ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        yalantinglibs
        GIT_REPOSITORY https://github.com/alibaba/yalantinglibs.git
        GIT_TAG v0.5.5
        GIT_SHALLOW 1
)

FetchContent_MakeAvailable(yalantinglibs)

# Expose as imported target (if not already provided)
if (NOT TARGET yalantinglibs::yalantinglibs)
    add_library(yalantinglibs::yalantinglibs INTERFACE IMPORTED)
    target_include_directories(yalantinglibs::yalantinglibs INTERFACE
            ${yalantinglibs_SOURCE_DIR}/include
            ${yalantinglibs_SOURCE_DIR}/standalone
            ${yalantinglibs_SOURCE_DIR}/thirdparty
    )
endif ()
