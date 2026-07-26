# Stratus Watch SDK Integration

# Path to the user projects folder
set(USER_PROJECTS_DIR "${CMAKE_CURRENT_LIST_DIR}/../user_projects")

# Check if the folder exists
if(EXISTS "${USER_PROJECTS_DIR}")
    # Find all C files in user_projects and its subdirectories
    file(GLOB_RECURSE USER_SRCS "${USER_PROJECTS_DIR}/*.c")
    
    if(USER_SRCS)
        message(STATUS "Found Stratus SDK user projects: ${USER_SRCS}")
        
        # Add the files to the current component (apps)
        target_sources(${COMPONENT_LIB} PRIVATE ${USER_SRCS})
        
        # Add the SDK include path
        target_include_directories(${COMPONENT_LIB} PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../sdk/include")
        
        # Also add the user_projects folder to include path just in case
        target_include_directories(${COMPONENT_LIB} PUBLIC "${USER_PROJECTS_DIR}")
    else()
        message(STATUS "No user apps found in ${USER_PROJECTS_DIR}")
    endif()
endif()
