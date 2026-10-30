# The plugin binary -- CMake twin of makefiles/ambuild/AMBuilder: the plugin's
# own sources, its include dirs and what it links with (the Source2Mod SDK's
# add_source2mod_plugin() adds the SDK side).
# Included by CMakeLists.txt after shared.cmake and the platform file.

# The plugin's own sources: everything under src/, globbed -- the fixes are
# ported in parallel, and a new src/fixes/<name>.cpp must not mean an edit of
# this file (nor of makefiles/ambuild/AMBuilder, which globs the same way).
# CONFIGURE_DEPENDS re-globs on every build, so a new file is picked up without
# touching CMake.
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(SORT SOURCE_FILES)

add_source2mod_plugin(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE
        # version_gen.h, from plugin-metadata.json (shared.cmake).
        ${VERSION_GEN_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # IGameFixes.h: the interface other plugins include.
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

if (LINK_LIBRARIES)
    target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARIES})
endif ()

# What the platform file asked for on the link line (the export map on Linux),
# like linux.ambuild / windows.ambuild's ConfigureBinary() on the AMBuild side.
if (PLUGIN_LINK_OPTIONS)
    target_link_options(${PROJECT_NAME} PRIVATE ${PLUGIN_LINK_OPTIONS})
endif ()
if (PLUGIN_LINK_DEPENDS)
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${PLUGIN_LINK_DEPENDS})
endif ()
