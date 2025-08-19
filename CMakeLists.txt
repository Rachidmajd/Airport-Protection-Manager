cmake_minimum_required(VERSION 3.16)
project(AeronauticalPlatform VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DRELEASE")
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(GDAL REQUIRED)
find_package(PkgConfig QUIET)

# Try to find ASIO for Crow
if(PKG_CONFIG_FOUND)
    pkg_check_modules(ASIO asio)
endif()

# Manual ASIO detection if pkg-config failed
if(NOT ASIO_FOUND)
    find_path(ASIO_INCLUDE_DIR 
        NAMES asio.hpp
        PATHS /usr/include /usr/local/include
    )
    
    if(ASIO_INCLUDE_DIR)
        set(ASIO_FOUND TRUE)
        set(ASIO_INCLUDE_DIRS ${ASIO_INCLUDE_DIR})
        message(STATUS "Found ASIO: ${ASIO_INCLUDE_DIR}")
    endif()
endif()

# Try to find MySQL Connector/C++ using pkg-config
if(PKG_CONFIG_FOUND)
    pkg_check_modules(MYSQLCPPCONN mysqlcppconn8)
endif()

# If pkg-config didn't find it, try manual detection for MySQL Connector/C++
if(NOT MYSQLCPPCONN_FOUND)
    find_path(MYSQLCPPCONN_INCLUDE_DIR 
        NAMES mysqlx/xdevapi.h mysql/jdbc.h
        PATHS /usr/include /usr/local/include /opt/mysql/include
        PATH_SUFFIXES mysql-cppconn8 mysql-connector-c++
    )
    
    find_library(MYSQLCPPCONN_LIBRARY 
        NAMES mysqlcppconn8 mysqlcppconn libmysqlcppconn8 libmysqlcppconn
        PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64 /opt/mysql/lib64 /opt/mysql/lib
    )
    
    if(MYSQLCPPCONN_INCLUDE_DIR AND MYSQLCPPCONN_LIBRARY)
        set(MYSQLCPPCONN_FOUND TRUE)
        set(MYSQLCPPCONN_LIBRARIES ${MYSQLCPPCONN_LIBRARY})
        set(MYSQLCPPCONN_INCLUDE_DIRS ${MYSQLCPPCONN_INCLUDE_DIR})
    endif()
endif()

# Fallback to standard MySQL C API if Connector/C++ not found
if(NOT MYSQLCPPCONN_FOUND)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(MYSQL mysqlclient)
    endif()
    
    if(NOT MYSQL_FOUND)
        find_path(MYSQL_INCLUDE_DIR 
            NAMES mysql/mysql.h mysql.h
            PATHS /usr/include /usr/local/include /usr/include/mysql
        )
        
        find_library(MYSQL_LIBRARY 
            NAMES mysqlclient libmysqlclient
            PATHS /usr/lib /usr/local/lib /usr/lib64 /usr/local/lib64
        )
        
        if(MYSQL_INCLUDE_DIR AND MYSQL_LIBRARY)
            set(MYSQL_FOUND TRUE)
            set(MYSQLCPPCONN_LIBRARIES ${MYSQL_LIBRARY})
            set(MYSQLCPPCONN_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
            set(MYSQLCPPCONN_FOUND TRUE)
            message(STATUS "Using MySQL C API instead of Connector/C++")
            add_compile_definitions(USE_MYSQL_C_API)
        endif()
    else()
        set(MYSQLCPPCONN_FOUND TRUE)
        set(MYSQLCPPCONN_LIBRARIES ${MYSQL_LIBRARIES})
        set(MYSQLCPPCONN_INCLUDE_DIRS ${MYSQL_INCLUDE_DIRS})
        message(STATUS "Using MySQL C API instead of Connector/C++")
        add_compile_definitions(USE_MYSQL_C_API)
    endif()
endif()

# Check if MySQL Connector/C++ is found (make it optional for now)
if(NOT MYSQLCPPCONN_FOUND)
    message(WARNING "MySQL Connector/C++ not found. Building without MySQL support.")
    message(WARNING "To enable MySQL: dnf install mysql-devel mysql++-devel")
    set(MYSQLCPPCONN_LIBRARIES "")
    set(MYSQLCPPCONN_INCLUDE_DIRS "")
    add_compile_definitions(NO_MYSQL_SUPPORT)
endif()


include_directories(${GDAL_INCLUDE_DIR})

# Source files (using GLOB_RECURSE to auto-detect files in src/)
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
)

# If no sources found in src/, fall back to current directory
if(NOT SOURCES)
    set(SOURCES
        main.cpp
        Project.cpp
        DatabaseManager.cpp
        ProjectRepository.cpp
        ProjectController.cpp
    )
endif()

# Create executable
add_executable(aeronautical_backend ${SOURCES})

# Set target properties
set_target_properties(aeronautical_backend PROPERTIES
    OUTPUT_NAME "aeronautical_backend"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Include directories for third-party header-only libraries
target_include_directories(aeronautical_backend PRIVATE
    # Current source directory
    ${CMAKE_CURRENT_SOURCE_DIR}
    # Crow framework - check both possible locations
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/crow/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/crow/include
    # nlohmann/json - check both possible locations
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/json/include
    # ASIO (required by Crow) - check multiple locations
    ${ASIO_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/asio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio
    # MySQL Connector/C++ (if needed)
    ${MYSQLCPPCONN_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(aeronautical_backend PRIVATE
    # System libraries
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${GDAL_LIBRARIES}
    # Logging
    spdlog::spdlog
    # Database
    ${MYSQLCPPCONN_LIBRARIES}
    # Compression
    z
)

# Compile definitions
target_compile_definitions(aeronautical_backend PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Create required directories
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/logs")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

# Optional: Add custom target for running the application
add_custom_target(run
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/bin/aeronautical_backend
    DEPENDS aeronautical_backend
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running aeronautical_backend..."
)

# Installation rules
install(TARGETS aeronautical_backend
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

# Install configuration files
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/config/config.json
    DESTINATION etc/aeronautical
    COMPONENT config
    OPTIONAL
)

# Install any additional resources
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/
        DESTINATION share/aeronautical/resources
        COMPONENT resources
        OPTIONAL
    )
endif()

# Create uninstall target (optional - only if template exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )
    
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstalling..."
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Build Configuration Summary ===")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Source files found: ${SOURCES}")
message(STATUS "OpenSSL version: ${OPENSSL_VERSION}")
message(STATUS "spdlog found: ${spdlog_FOUND}")
message(STATUS "MySQL Connector/C++ found: ${MYSQLCPPCONN_FOUND}")
if(MYSQLCPPCONN_FOUND)
    message(STATUS "MySQL Connector/C++ libraries: ${MYSQLCPPCONN_LIBRARIES}")
endif()
message(STATUS "===================================")
message(STATUS "")
