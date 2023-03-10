flux_add_shell_plugin ( datastaging data-staging.cpp)
target_link_libraries ( datastaging PRIVATE
    Boost::filesystem
    )

