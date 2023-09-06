flux_add_plugin ( datastaging SHELL data-staging.cpp)
target_link_libraries ( datastaging PRIVATE
    Boost::filesystem
    )

