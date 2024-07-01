include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR})

add_library(stasis_core STATIC
        globals.c
        str.c
        strlist.c
        ini.c
        conda.c
        environment.c
        utils.c
        system.c
        download.c
        delivery.c
        recipe.c
        relocation.c
        wheel.c
        copy.c
        artifactory.c
        template.c
        rules.c
        docker.c
        junitxml.c
        github.c
        template_func_proto.c
)

add_executable(stasis
        stasis_main.c
)
target_link_libraries(stasis PRIVATE stasis_core)
target_link_libraries(stasis PUBLIC LibXml2::LibXml2)
add_executable(stasis_indexer
        stasis_indexer.c
)
target_link_libraries(stasis_indexer PRIVATE stasis_core)
install(TARGETS stasis stasis_indexer RUNTIME)
