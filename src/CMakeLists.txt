include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR})

add_executable(omc
        main.c
        str.c
        strlist.c
        ini.c
        conda.c
        environment.c
        utils.c
        system.c
        download.c
        deliverable.c
        recipe.c
        relocation.c
        wheel.c
        copy.c
        artifactory.c
        template.c
        rules.c
        docker.c
)
install(TARGETS omc RUNTIME)
