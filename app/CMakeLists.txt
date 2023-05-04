
file(GLOB_RECURSE CppSources ${CMAKE_SOURCE_DIR}/app/*.cpp)
set(CppHeaders "${CMAKE_SOURCE_DIR}/app")

add_executable(app ${CppSources})

# Add threading for MSVC compilation
if ( MSVC )
    target_compile_options(app PRIVATE "/MP")
endif( MSVC )

## Add headers
target_include_directories(app
    PRIVATE
    ${CppHeaders}
)

## Add libs
if ( MSVC )
    
    target_link_libraries(app PRIVATE glfw3)
    target_link_libraries(app PRIVATE ${Vulkan_LIBRARIES})
    target_link_libraries(app PRIVATE VulkanMemoryAllocator)


elseif ( APPLE )
    
    target_link_libraries(app PRIVATE glfw)

endif ( MSVC )

## Compilation features
target_compile_features(app PRIVATE cxx_std_20)

### Copy resources

##Shaders

#We get all shader files
file(GLOB_RECURSE shaders RELATIVE ${CMAKE_SOURCE_DIR}/resources ${CMAKE_SOURCE_DIR}/resources/*spv)

message("Here debug : ${CMAKE_SOURCE_DIR}/resources  ${shaders}") 

#We create a custom command to generate all our shaders in the output dir
add_custom_command( OUTPUT ${shaders} COMMAND)

#We specify that the name in the output is symbolic and we will not create a file exactly on this path
set_source_files_properties(${shaders} PROPERTIES SYMBOLIC 1)


foreach(shader IN LISTS shaders)

    #We add the copy for each shader
    add_custom_command(OUTPUT ${shaders}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/${shader}
            $<TARGET_FILE_DIR:app>/${shader}
        DEPENDS ${CMAKE_SOURCE_DIR}/resources/${shader}
        APPEND
    )

endforeach()

#We create a custom target to then make the app depend of it
add_custom_target(copyShaders ALL DEPENDS ${shaders})
add_dependencies(app copyShaders)
