cmake_minimum_required(VERSION 3.24)
project(EcoSphereLegible VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)

# Esta carpeta usa los modelos, librerías, SDK y pruebas del proyecto original.
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(TESTS "${ROOT}/tests")
set(BIN "${CMAKE_CURRENT_BINARY_DIR}")

set(ECOS_GLFW_VERSION "3.5.1" CACHE STRING "Versión exacta de GLFW")
find_package(glfw3 ${ECOS_GLFW_VERSION} EXACT CONFIG REQUIRED)

# ---------- Librerías de terceros ----------
add_library(ecos_glm INTERFACE)
target_include_directories(ecos_glm SYSTEM INTERFACE ${ROOT}/third_party/glm)

add_library(ecos_glad STATIC ${ROOT}/third_party/glad/src/gl.c)
target_include_directories(ecos_glad SYSTEM PUBLIC ${ROOT}/third_party/glad/include)

add_library(ecos_asset_formats STATIC ${SRC}/engine/assets/ThirdParty.c)
target_include_directories(ecos_asset_formats SYSTEM PUBLIC ${ROOT}/third_party/cgltf ${ROOT}/third_party/stb)
if(UNIX)
    target_link_libraries(ecos_asset_formats PUBLIC m)
endif()

# ---------- Motor ----------
add_library(ecos_module_api INTERFACE)
target_include_directories(ecos_module_api INTERFACE ${ROOT}/include)
target_compile_features(ecos_module_api INTERFACE cxx_std_17)
target_link_libraries(ecos_module_api INTERFACE ecos_glm)

add_library(ecoscore STATIC ${SRC}/engine/core/ModuleSession.cpp)
target_link_libraries(ecoscore PUBLIC ecos_module_api)

add_library(ecosscene STATIC
    ${SRC}/engine/scene/Scene.cpp
    ${SRC}/engine/camera/Camera.cpp
    ${SRC}/engine/animation/Animation.cpp
    ${SRC}/engine/interaction/Interaction.cpp)
target_include_directories(ecosscene PUBLIC ${ROOT}/include ${SRC}/engine ${SRC})
target_link_libraries(ecosscene PUBLIC ecos_glm)
target_link_libraries(ecos_module_api INTERFACE ecosscene)

add_library(ecosassets STATIC
    ${SRC}/engine/assets/AssetManager.cpp
    ${SRC}/world/LightingDemo.cpp
    ${SRC}/world/InteractionAnimationDemo.cpp)
target_link_libraries(ecosassets PUBLIC ecosscene PRIVATE ecos_asset_formats)

add_library(ecoscontent STATIC ${SRC}/engine/content/ContentRegistry.cpp)
target_link_libraries(ecoscontent PUBLIC ecosassets ecos_module_api)

add_library(ecosgraphics STATIC
    ${SRC}/engine/graphics/Triangle.cpp
    ${SRC}/engine/graphics/SceneRenderer.cpp
    ${SRC}/engine/graphics/ModelRenderer.cpp
    ${SRC}/engine/graphics/Environment.cpp)
target_include_directories(ecosgraphics PUBLIC ${SRC}/engine/graphics)
target_link_libraries(ecosgraphics PUBLIC ecos_glad ecosscene ecosassets)

# ---------- Zonas ----------
add_library(ecosshared STATIC ${SRC}/shared/SharedResources.cpp)
target_include_directories(ecosshared PUBLIC ${SRC})
target_link_libraries(ecosshared PUBLIC ecos_module_api)

foreach(module IN ITEMS EcoEnergy AquaBio EcoCycle)
    add_library(${module} STATIC ${SRC}/modules/${module}/Module.cpp)
    target_link_libraries(${module} PUBLIC ecos_module_api ecosshared)
endforeach()

add_library(ecosplaza STATIC ${SRC}/world/Plaza.cpp)
target_link_libraries(ecosplaza PUBLIC ecos_module_api ecosshared)

add_library(ecosnavigation STATIC ${SRC}/world/ZoneNavigator.cpp)
target_link_libraries(ecosnavigation PUBLIC ecosplaza ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)

# ---------- Programa ----------
set(HUD ${SRC}/app/NavigationHud.cpp ${SRC}/app/NavigationHudLayout.cpp)
set(INPUT ${SRC}/app/InteractionDemoInput.cpp)

add_executable(ecosphere
    ${SRC}/app/main.cpp
    ${SRC}/app/LightingSmoke.cpp
    ${SRC}/app/InteractionSmoke.cpp
    ${SRC}/app/ContentSmoke.cpp
    ${SRC}/app/PlazaSmoke.cpp
    ${SRC}/app/EcoEnergySmoke.cpp
    ${SRC}/app/AquaBioSmoke.cpp
    ${SRC}/app/EcoCycleSmoke.cpp
    ${SRC}/app/NavigationSmoke.cpp
    ${HUD} ${INPUT})
target_compile_definitions(ecosphere PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(ecosphere PRIVATE ecosnavigation ecosgraphics glfw)

# Shaders y modelos se copian junto al ejecutable en cada compilación.
set(MODELS "${ROOT}/assets")
add_custom_target(ecos_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADERS}" "$<TARGET_FILE_DIR:ecosphere>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MODELS}/common/models" "$<TARGET_FILE_DIR:ecosphere>/assets/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MODELS}/plaza/models" "$<TARGET_FILE_DIR:ecosphere>/assets/plaza/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MODELS}/installations/ecoenergy/models" "$<TARGET_FILE_DIR:ecosphere>/assets/ecoenergy/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MODELS}/installations/aquabio/models" "$<TARGET_FILE_DIR:ecosphere>/assets/aquabio/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MODELS}/installations/ecocycle/models" "$<TARGET_FILE_DIR:ecosphere>/assets/ecocycle/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${MODELS}/shared/models" "$<TARGET_FILE_DIR:ecosphere>/assets/shared/models"
    VERBATIM)
add_dependencies(ecosphere ecos_shaders)

set(ecos_targets ecoscore ecosscene ecosassets ecoscontent ecosgraphics ecosshared ecosplaza ecosnavigation
    EcoEnergy AquaBio EcoCycle ecosphere)

# ---------- Pruebas (las mismas del proyecto original) ----------
if(BUILD_TESTING)
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    set(PY "${Python3_EXECUTABLE}")
    set(ZONES ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)

    # ecos_test(nombre [EXTRA archivos...] LIBS librerías...)
    function(ecos_test name)
        cmake_parse_arguments(T "" "" "EXTRA;LIBS" ${ARGN})
        add_executable(${name} ${TESTS}/${name}.cpp ${T_EXTRA})
        target_compile_definitions(${name} PRIVATE GLFW_INCLUDE_NONE)
        target_include_directories(${name} SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
        target_link_libraries(${name} PRIVATE ${T_LIBS})
        add_dependencies(${name} ecos_shaders)
        set(ecos_targets ${ecos_targets} ${name} PARENT_SCOPE)
    endfunction()

    function(ecos_must_fail name)
        add_test(NAME ${name} COMMAND ecosphere ${ARGN})
        set_tests_properties(${name} PROPERTIES WILL_FAIL TRUE)
    endfunction()

    # Pruebas con ventana: se compilan pero no corren en CTest.
    ecos_test(texture_visibility_graphics_tests LIBS ecosnavigation ecosgraphics glfw)
    ecos_test(shading_graphics_tests EXTRA ${HUD} LIBS ecosnavigation ecosgraphics glfw)
    ecos_test(lighting_hud_graphics_tests EXTRA ${HUD} LIBS ecosnavigation ecosgraphics glfw)
    ecos_test(fps_graphics_tests EXTRA ${HUD} LIBS ecosnavigation ecosgraphics glfw)

    # Controles, HUD y navegación
    ecos_test(interaction_scale_tests EXTRA ${INPUT} LIBS ecosnavigation)
    add_test(NAME interaction_scale_limits_cpu COMMAND interaction_scale_tests "${BIN}/assets")
    ecos_test(shading_tests EXTRA ${SRC}/app/NavigationHudLayout.cpp LIBS ecosnavigation glfw)
    add_test(NAME shading_cpu COMMAND shading_tests "${BIN}/assets")
    ecos_test(lighting_hud_tests EXTRA ${SRC}/app/NavigationHudLayout.cpp LIBS ecosnavigation glfw)
    add_test(NAME lighting_hud_cpu COMMAND lighting_hud_tests "${BIN}/assets")
    ecos_test(fps_tests EXTRA ${SRC}/app/NavigationHudLayout.cpp LIBS ecosassets glfw)
    add_test(NAME fps_cpu COMMAND fps_tests)
    ecos_test(visitor_controls_tests EXTRA ${INPUT} ${SRC}/app/NavigationHudLayout.cpp LIBS ecosnavigation)
    add_test(NAME visitor_controls_cpu COMMAND visitor_controls_tests "${BIN}/assets")
    ecos_test(zone_navigation_tests LIBS ecosnavigation)
    add_test(NAME zone_navigation_cpu COMMAND zone_navigation_tests "${BIN}/assets")

    # Texturas
    add_test(NAME presentation_textures_integrity COMMAND "${PY}" -B "${ROOT}/tools/apply_presentation_textures.py" --check)
    add_test(NAME presentation_textures_regeneration COMMAND "${PY}" -B "${TESTS}/presentation_textures_regeneration.py")

    # Recursos compartidos
    ecos_test(shared_resources_tests LIBS ecosnavigation)
    add_test(NAME shared_resources_cpu COMMAND shared_resources_tests "${BIN}/assets"
        "${ROOT}/docs/assets/SHARED_INSTANCES.csv" "${BIN}/shared-import.csv" "${BIN}/shared-space-bounds.csv")
    set_tests_properties(shared_resources_cpu PROPERTIES FIXTURES_SETUP shared_import)
    add_test(NAME shared_inventory COMMAND "${PY}" -B "${ROOT}/tools/validate_shared_inventory.py"
        --root "${ROOT}" --import-report "${BIN}/shared-import.csv" --self-test)
    add_test(NAME shared_spatial_layout COMMAND "${PY}" -B "${ROOT}/tools/validate_shared_space.py"
        --root "${ROOT}" --bounds "${BIN}/shared-space-bounds.csv" --report "${BIN}/shared-space-audit.json" --self-test)
    set_tests_properties(shared_inventory shared_spatial_layout PROPERTIES FIXTURES_REQUIRED shared_import)

    # Plaza
    ecos_test(plaza_tests LIBS ecosplaza ${ZONES})
    add_test(NAME plaza_assets_cpu COMMAND plaza_tests "${BIN}/assets"
        "${ROOT}/docs/assets/PLAZA_INSTANCES.csv" "${BIN}/plaza-import.csv" "${BIN}/plaza-scratch")
    set_tests_properties(plaza_assets_cpu PROPERTIES FIXTURES_SETUP plaza_import)
    add_test(NAME plaza_inventory COMMAND "${PY}" "${ROOT}/tools/validate_plaza_inventory.py"
        --root "${ROOT}" --import-report "${BIN}/plaza-import.csv"
        --usage-report "${ROOT}/docs/evidence/phase5d/plaza/model-use.csv" --require-definitive --self-test)
    set_tests_properties(plaza_inventory PROPERTIES FIXTURES_REQUIRED plaza_import)

    # SDK para módulos externos
    ecos_test(sdk_v1_tests EXTRA ${TESTS}/sdk_consumer/LessonModule.cpp LIBS ecoscontent ecoscore)
    add_test(NAME sdk_v1_cpu COMMAND sdk_v1_tests "${BIN}/assets")
    add_test(NAME sdk_external_consumer COMMAND "${PY}" "${TESTS}/sdk_external_test.py"
        --root "${ROOT}" --build "${BIN}/sdk-external"
        --core "$<TARGET_FILE:ecoscore>" --scene "$<TARGET_FILE:ecosscene>" --compiler "${CMAKE_CXX_COMPILER}"
        "--flags=${CMAKE_CXX_FLAGS}" "--link-flags=${CMAKE_EXE_LINKER_FLAGS}")

    # Las tres instalaciones
    ecos_test(ecoenergy_tests LIBS ecosplaza ${ZONES})
    add_test(NAME ecoenergy_assets_cpu COMMAND ecoenergy_tests "${BIN}/assets"
        "${ROOT}/docs/assets/ECOENERGY_INSTANCES.csv" "${BIN}/ecoenergy-import.csv" "${BIN}/ecoenergy-scratch")
    set_tests_properties(ecoenergy_assets_cpu PROPERTIES FIXTURES_SETUP ecoenergy_import)
    add_test(NAME ecoenergy_inventory COMMAND "${PY}" "${ROOT}/tools/validate_ecoenergy_inventory.py"
        --root "${ROOT}" --import-report "${BIN}/ecoenergy-import.csv"
        --usage-report "${ROOT}/docs/evidence/phase6a/ecoenergy/model-use.csv" --require-definitive --self-test)
    set_tests_properties(ecoenergy_inventory PROPERTIES FIXTURES_REQUIRED ecoenergy_import)

    ecos_test(aquabio_tests LIBS ecosplaza ${ZONES})
    add_test(NAME aquabio_assets_cpu COMMAND aquabio_tests "${BIN}/assets"
        "${ROOT}/docs/assets/AQUABIO_INSTANCES.csv" "${BIN}/aquabio-import.csv" "${BIN}/aquabio-scratch")
    set_tests_properties(aquabio_assets_cpu PROPERTIES FIXTURES_SETUP aquabio_import)
    add_test(NAME aquabio_inventory COMMAND "${PY}" "${ROOT}/tools/validate_aquabio_inventory.py"
        --root "${ROOT}" --import-report "${BIN}/aquabio-import.csv"
        --usage-report "${ROOT}/docs/evidence/aquabio-pond-animation/model-use.csv" --require-definitive --self-test)
    add_test(NAME aquabio_spatial_layout COMMAND "${PY}" "${ROOT}/tools/validate_aquabio_space.py"
        --root "${ROOT}" --bounds "${BIN}/aquabio-space-bounds.csv"
        --views "${ROOT}/docs/evidence/aquabio-pond-animation/views.csv"
        --report "${BIN}/aquabio-space-audit.json" --self-test)
    set_tests_properties(aquabio_inventory aquabio_spatial_layout PROPERTIES FIXTURES_REQUIRED aquabio_import)

    ecos_test(ecocycle_tests LIBS ecosplaza ${ZONES})
    add_test(NAME ecocycle_assets_cpu COMMAND ecocycle_tests "${BIN}/assets"
        "${ROOT}/docs/assets/ECOCYCLE_INSTANCES.csv" "${BIN}/ecocycle-import.csv" "${BIN}/ecocycle-scratch")
    set_tests_properties(ecocycle_assets_cpu PROPERTIES FIXTURES_SETUP ecocycle_import)
    add_test(NAME ecocycle_inventory COMMAND "${PY}" "${ROOT}/tools/validate_ecocycle_inventory.py"
        --root "${ROOT}" --import-report "${BIN}/ecocycle-import.csv" --self-test --require-definitive)
    add_test(NAME ecocycle_spatial_layout COMMAND "${PY}" "${ROOT}/tools/validate_ecocycle_space.py"
        --root "${ROOT}" --bounds "${BIN}/ecocycle-space-bounds.csv" --report "${BIN}/ecocycle-space-audit.json" --self-test)
    set_tests_properties(ecocycle_inventory ecocycle_spatial_layout PROPERTIES FIXTURES_REQUIRED ecocycle_import)

    # Contenido, módulos y motor
    ecos_test(content_registry_tests LIBS ecoscontent ecoscore)
    add_test(NAME content_registry_cpu COMMAND content_registry_tests "${ROOT}/assets/common" "${BIN}/content-scratch")
    ecos_test(content_module_tests EXTRA ${INPUT} LIBS ${ZONES} glfw)
    add_test(NAME content_modules_integration_cpu COMMAND content_module_tests "${BIN}/assets")
    ecos_test(module_contract_tests LIBS ecoscore EcoEnergy AquaBio EcoCycle)
    add_test(NAME module_contract COMMAND module_contract_tests)
    ecos_test(graphics_failure_tests LIBS ecosgraphics)
    add_test(NAME graphics_failure_cleanup COMMAND graphics_failure_tests "${SHADERS}")
    ecos_test(scene_camera_tests LIBS ecosscene)
    add_test(NAME scene_camera_cpu COMMAND scene_camera_tests)
    ecos_test(asset_tests LIBS ecosassets)
    add_test(NAME assets_cpu COMMAND asset_tests "${BIN}/asset-test-scratch" "${ROOT}/assets/common/models")
    ecos_test(lighting_tests LIBS ecosassets)
    add_test(NAME lighting_cpu COMMAND lighting_tests)
    ecos_test(lighting_asset_tests LIBS ecosassets)
    add_test(NAME lighting_assets_cpu COMMAND lighting_asset_tests "${BIN}/lighting-asset-scratch")
    ecos_test(interaction_animation_tests LIBS ecosassets)
    add_test(NAME interaction_animation_cpu COMMAND interaction_animation_tests "${ROOT}/assets/common")

    # La línea de comandos debe rechazar combinaciones inválidas.
    ecos_must_fail(cli_rejects_unscoped_benchmark --benchmark-no-vsync)
    ecos_must_fail(plaza_cli_rejects_module --plaza --module ecoenergy)
    ecos_must_fail(cli_rejects_aquabio_module --aquabio-smoke unused --module aquabio)
    ecos_must_fail(cli_rejects_two_installation_smokes --aquabio-smoke unused --ecoenergy-smoke unused)
    ecos_must_fail(cli_rejects_ecocycle_module --ecocycle-smoke unused --module ecocycle)
    ecos_must_fail(cli_rejects_ecocycle_mixed --ecocycle-smoke unused --aquabio-smoke unused)
    ecos_must_fail(cli_rejects_unknown_module --module missing)
    ecos_must_fail(cli_rejects_unknown_option --unknown)
endif()

foreach(target IN LISTS ecos_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
