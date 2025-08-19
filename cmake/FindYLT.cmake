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

#add_executable(main main.cpp)
#target_link_libraries(main yalantinglibs::yalantinglibs)
#target_compile_features(main PRIVATE cxx_std_20)