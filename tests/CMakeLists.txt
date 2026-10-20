# Enables CTest
enable_testing()

add_executable(MoleTests
        test_main.cpp
        test_test.cpp
        transform_manager_test.cpp
        material_pool_test.cpp
        material_packer_test.cpp
        file_system_test.cpp
)

target_link_libraries(MoleTests
        PRIVATE
        MoleCore
        doctest::doctest
)

add_test(NAME MoleTestSuite COMMAND MoleTests)