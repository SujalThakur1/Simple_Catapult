cmake_minimum_required(VERSION 3.10)
project(SimpleCatapult)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Define GLEW_STATIC for Windows static linking ---
if(WIN32)
    add_definitions(-DGLEW_STATIC)
endif()

# --- OpenGL ---
find_package(OpenGL REQUIRED)

# --- GLFW Setup ---
set(GLFW_FOUND FALSE)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/glfw/include/GLFW/glfw3.h")
    message(STATUS "Found GLFW in project folder")
    set(GLFW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/glfw/include")
    if(WIN32)
        set(GLFW_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/glfw/lib-vc2022/glfw3.lib")
    else()
        find_library(GLFW_LIBRARY NAMES glfw3 PATHS "${CMAKE_SOURCE_DIR}/third_party/glfw/lib" NO_DEFAULT_PATH)
    endif()

    if(GLFW_LIBRARY)
        add_library(glfw UNKNOWN IMPORTED)
        set_target_properties(glfw PROPERTIES
            IMPORTED_LOCATION "${GLFW_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
        )
        set(GLFW_FOUND TRUE)
    endif()
endif()

if(NOT GLFW_FOUND)
    find_package(glfw3 QUIET)
    if(glfw3_FOUND)
        set(GLFW_FOUND TRUE)
        message(STATUS "Found GLFW system-wide")
    endif()
endif()

if(NOT GLFW_FOUND)
    message(FATAL_ERROR "GLFW not found! Put it in third_party/glfw or install system-wide.")
endif()

# --- GLEW Setup ---
if(WIN32)
    set(GLEW_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/glew/include")
    set(GLEW_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/glew/lib/Release/x64/glew32s.lib")
elseif(APPLE)
    set(GLEW_INCLUDE_DIR /opt/homebrew/include)
    set(GLEW_LIBRARY /opt/homebrew/lib/libGLEW.dylib)
else()
    find_package(GLEW REQUIRED)
endif()

# --- GLM Setup ---
set(GLM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/glm")

# --- Assimp Setup ---
if(APPLE)
    set(ASSIMP_INCLUDE_DIR /opt/homebrew/include)
    set(ASSIMP_LIBRARY /opt/homebrew/lib/libassimp.dylib)
elseif(UNIX AND NOT APPLE)
    find_package(assimp REQUIRED)
    set(ASSIMP_INCLUDE_DIR ${ASSIMP_INCLUDE_DIRS})
    set(ASSIMP_LIBRARY ${ASSIMP_LIBRARIES})
else() # Windows
    set(ASSIMP_ROOT "${CMAKE_SOURCE_DIR}/third_party/assimp")
    set(ASSIMP_INCLUDE_DIR "${ASSIMP_ROOT}/include")
    set(ASSIMP_LIB_DIR "${ASSIMP_ROOT}/lib")
    set(ASSIMP_BIN_DIR "${ASSIMP_ROOT}/bin")

    # ←←← THIS IS THE CORRECT WAY FOR DLL VERSION ←←←
    add_library(assimp SHARED IMPORTED)               # ← SHARED, not STATIC
    set_target_properties(assimp PROPERTIES
        IMPORTED_LOCATION "${ASSIMP_LIB_DIR}/assimp-vc143-mt.dll"          # .dll for runtime
        IMPORTED_IMPLIB   "${ASSIMP_LIB_DIR}/assimp-vc143-mt.lib"          # .lib for linking
        INTERFACE_INCLUDE_DIRECTORIES "${ASSIMP_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "ASSIMP_DLL"                         # ← this fixes 90% of errors
    )
    set(ASSIMP_LIBRARY assimp)
endif()

# --- Include Directories ---
include_directories(
    ${OPENGL_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIR}
    include
)

# --- Executable ---
add_executable(SimpleCatapult
    src/main.cpp
    src/Catapult.cpp
    src/Camera.cpp
    src/Terrain.cpp
    src/Projectile.cpp
    src/Skybox.cpp
    src/Model.cpp
    src/Zombie.cpp
    src/stb_image_impl.cpp
)

# --- Copy ALL required Assimp + dependency DLLs automatically (Windows only) ---
if(WIN32)
    set(ASSIMP_DLLS
        "${ASSIMP_BIN_DIR}/assimp-vc143-mt.dll"
        "${ASSIMP_BIN_DIR}/poly2tri.dll"
        "${ASSIMP_BIN_DIR}/zlibd1.dll"
        "${ASSIMP_BIN_DIR}/minizip.dll"
        "${ASSIMP_BIN_DIR}/kubazip.dll"
        "${ASSIMP_BIN_DIR}/pugixml.dll"
    )

    foreach(dll ${ASSIMP_DLLS})
        if(EXISTS "${dll}")
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "Copying ${dll} to output directory"
            )
        endif()
    endforeach()
endif()

# --- Link Libraries ---
target_link_libraries(SimpleCatapult
    ${OPENGL_LIBRARIES}
    glfw
    ${GLEW_LIBRARY}
    ${ASSIMP_LIBRARY}
)

# --- macOS Frameworks ---
if(APPLE)
    target_link_libraries(SimpleCatapult
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()
