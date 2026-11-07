cmake_minimum_required(VERSION 3.24)
project(EcoSphere VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)

# GLFW se prepara explícitamente; configurar nunca accede a la red.
set(ECOS_GLFW_VERSION "3.5.1" CACHE STRING "Exact provisioned GLFW version")
find_package(glfw3 ${ECOS_GLFW_VERSION} EXACT CONFIG REQUIRED)

add_library(ecos_glm INTERFACE)
target_include_directories(ecos_glm SYSTEM INTERFACE third_party/glm)
add_library(ecos_glad STATIC third_party/glad/src/gl.c)
target_include_directories(ecos_glad SYSTEM PUBLIC third_party/glad/include)

# El SDK solo contiene contratos CPU; no arrastra GLFW ni OpenGL a los módulos.
add_library(ecos_module_api INTERFACE)
target_include_directories(ecos_module_api INTERFACE include)
target_compile_features(ecos_module_api INTERFACE cxx_std_17)
target_link_libraries(ecos_module_api INTERFACE ecos_glm)

add_library(ecoscore STATIC src/engine/core/ModuleSession.cpp)
target_link_libraries(ecoscore PUBLIC ecos_module_api)

add_library(ecosshared STATIC src/shared/SharedResources.cpp)
target_link_libraries(ecosshared PUBLIC ecos_module_api)
target_include_directories(ecosshared PUBLIC src)

# Cada instalación puede compilar su biblioteca por separado.
foreach(module IN ITEMS EcoEnergy AquaBio EcoCycle)
    add_library(${module} STATIC src/modules/${module}/Module.cpp)
    target_link_libraries(${module} PUBLIC ecos_module_api ecosshared)
endforeach()

add_library(ecosscene STATIC src/engine/scene/Scene.cpp src/engine/camera/Camera.cpp src/engine/animation/Animation.cpp src/engine/interaction/Interaction.cpp)
target_include_directories(ecosscene PUBLIC include src/engine src)
target_link_libraries(ecosscene PUBLIC ecos_glm)
target_link_libraries(ecos_module_api INTERFACE ecosscene)

add_library(ecos_asset_formats STATIC src/engine/assets/ThirdParty.c)
target_include_directories(ecos_asset_formats SYSTEM PUBLIC third_party/cgltf third_party/stb)
if(UNIX)
    target_link_libraries(ecos_asset_formats PUBLIC m)
endif()
add_library(ecosassets STATIC src/engine/assets/AssetManager.cpp src/world/LightingDemo.cpp src/world/InteractionAnimationDemo.cpp)
target_include_directories(ecosassets PUBLIC src/engine src)
target_link_libraries(ecosassets PUBLIC ecosscene PRIVATE ecos_asset_formats)

add_library(ecoscontent STATIC src/engine/content/ContentRegistry.cpp)
target_link_libraries(ecoscontent PUBLIC ecosassets ecos_module_api)
add_library(ecosplaza STATIC src/world/Plaza.cpp)
target_include_directories(ecosplaza PUBLIC src)
target_link_libraries(ecosplaza PUBLIC ecos_module_api ecosshared)
add_library(ecosnavigation STATIC src/world/ZoneNavigator.cpp)
target_link_libraries(ecosnavigation PUBLIC ecosplaza ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)

add_library(ecosgraphics STATIC src/engine/graphics/Triangle.cpp src/engine/graphics/SceneRenderer.cpp src/engine/graphics/ModelRenderer.cpp src/engine/graphics/Environment.cpp)
target_link_libraries(ecosgraphics PUBLIC ecos_glad ecosscene ecosassets)
target_include_directories(ecosgraphics PUBLIC src/engine/graphics)

add_executable(ecosphere src/app/main.cpp src/app/LightingSmoke.cpp src/app/InteractionDemoInput.cpp src/app/InteractionSmoke.cpp src/app/ContentSmoke.cpp src/app/PlazaSmoke.cpp src/app/EcoEnergySmoke.cpp src/app/AquaBioSmoke.cpp src/app/EcoCycleSmoke.cpp src/app/NavigationHud.cpp src/app/NavigationHudLayout.cpp src/app/NavigationSmoke.cpp)
target_compile_definitions(ecosphere PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(ecosphere PRIVATE ecoscore ecoscontent ecosplaza ecosnavigation ecosgraphics glfw EcoEnergy AquaBio EcoCycle)

# Copia offline junto al ejecutable; cada build actualiza shaders editados en clase.
add_custom_target(ecos_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ecosphere>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders/scene.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders/scene.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders/model.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders/model.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders/skybox.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders/skybox.frag"
        "$<TARGET_FILE_DIR:ecosphere>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/models"
        "$<TARGET_FILE_DIR:ecosphere>/assets/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/plaza/models"
        "$<TARGET_FILE_DIR:ecosphere>/assets/plaza/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/installations/ecoenergy/models"
        "$<TARGET_FILE_DIR:ecosphere>/assets/ecoenergy/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/installations/aquabio/models"
        "$<TARGET_FILE_DIR:ecosphere>/assets/aquabio/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/installations/ecocycle/models"
        "$<TARGET_FILE_DIR:ecosphere>/assets/ecocycle/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shared/models"
        "$<TARGET_FILE_DIR:ecosphere>/assets/shared/models"
    VERBATIM)
add_dependencies(ecosphere ecos_shaders)

set(ecos_targets ecosassets ecoscore ecosshared ecosscene ecoscontent ecosplaza ecosnavigation ecosgraphics ecosphere EcoEnergy AquaBio EcoCycle)
if(BUILD_TESTING)
    add_executable(shading_graphics_tests tests/shading_graphics_tests.cpp src/app/NavigationHud.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(shading_graphics_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(shading_graphics_tests PRIVATE ecosnavigation ecosgraphics glfw)
    add_dependencies(shading_graphics_tests ecos_shaders)
    list(APPEND ecos_targets shading_graphics_tests)
    add_executable(shading_tests tests/shading_tests.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(shading_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(shading_tests PRIVATE ecosnavigation glfw)
    add_dependencies(shading_tests ecos_shaders)
    add_test(NAME shading_cpu COMMAND shading_tests "${CMAKE_CURRENT_BINARY_DIR}/assets")
    list(APPEND ecos_targets shading_tests)
    add_executable(lighting_hud_graphics_tests tests/lighting_hud_graphics_tests.cpp src/app/NavigationHud.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(lighting_hud_graphics_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(lighting_hud_graphics_tests PRIVATE ecosnavigation ecosgraphics glfw)
    add_dependencies(lighting_hud_graphics_tests ecos_shaders)
    list(APPEND ecos_targets lighting_hud_graphics_tests)
    add_executable(lighting_hud_tests tests/lighting_hud_tests.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(lighting_hud_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(lighting_hud_tests PRIVATE ecosnavigation glfw)
    add_dependencies(lighting_hud_tests ecos_shaders)
    add_test(NAME lighting_hud_cpu COMMAND lighting_hud_tests "${CMAKE_CURRENT_BINARY_DIR}/assets")
    list(APPEND ecos_targets lighting_hud_tests)
    # Opt-in real-context checks; keep headless CTest usable.
    add_executable(fps_graphics_tests tests/fps_graphics_tests.cpp src/app/NavigationHud.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(fps_graphics_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(fps_graphics_tests PRIVATE ecosnavigation ecosgraphics glfw)
    add_dependencies(fps_graphics_tests ecos_shaders)
    list(APPEND ecos_targets fps_graphics_tests)
    add_executable(fps_tests tests/fps_tests.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(fps_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(fps_tests PRIVATE ecosassets glfw)
    add_test(NAME fps_cpu COMMAND fps_tests)
    list(APPEND ecos_targets fps_tests)
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    add_test(NAME presentation_textures_integrity COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/apply_presentation_textures.py" --check)
    add_executable(visitor_controls_tests tests/visitor_controls_tests.cpp src/app/InteractionDemoInput.cpp src/app/NavigationHudLayout.cpp)
    target_compile_definitions(visitor_controls_tests PRIVATE GLFW_INCLUDE_NONE)
    target_include_directories(visitor_controls_tests SYSTEM PRIVATE $<TARGET_PROPERTY:glfw,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(visitor_controls_tests PRIVATE ecosnavigation)
    add_dependencies(visitor_controls_tests ecos_shaders)
    add_test(NAME visitor_controls_cpu COMMAND visitor_controls_tests "${CMAKE_CURRENT_BINARY_DIR}/assets")
    list(APPEND ecos_targets visitor_controls_tests)
    add_executable(shared_resources_tests tests/shared_resources_tests.cpp)
    target_link_libraries(shared_resources_tests PRIVATE ecosnavigation)
    add_dependencies(shared_resources_tests ecos_shaders)
    add_test(NAME shared_resources_cpu COMMAND shared_resources_tests "${CMAKE_CURRENT_BINARY_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/assets/SHARED_INSTANCES.csv" "${CMAKE_CURRENT_BINARY_DIR}/shared-import.csv"
        "${CMAKE_CURRENT_BINARY_DIR}/shared-space-bounds.csv")
    set_tests_properties(shared_resources_cpu PROPERTIES FIXTURES_SETUP shared_import)
    add_test(NAME shared_inventory COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_shared_inventory.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --import-report "${CMAKE_CURRENT_BINARY_DIR}/shared-import.csv" --self-test)
    add_test(NAME shared_spatial_layout COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_shared_space.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --bounds "${CMAKE_CURRENT_BINARY_DIR}/shared-space-bounds.csv"
        --report "${CMAKE_CURRENT_BINARY_DIR}/shared-space-audit.json" --self-test)
    set_tests_properties(shared_inventory shared_spatial_layout PROPERTIES FIXTURES_REQUIRED shared_import)
    list(APPEND ecos_targets shared_resources_tests)
    add_executable(zone_navigation_tests tests/zone_navigation_tests.cpp)
    target_link_libraries(zone_navigation_tests PRIVATE ecosnavigation)
    add_dependencies(zone_navigation_tests ecos_shaders)
    add_test(NAME zone_navigation_cpu COMMAND zone_navigation_tests "${CMAKE_CURRENT_BINARY_DIR}/assets")
    add_test(NAME cli_rejects_unscoped_benchmark COMMAND ecosphere --benchmark-no-vsync)
    set_tests_properties(cli_rejects_unscoped_benchmark PROPERTIES WILL_FAIL TRUE)
    list(APPEND ecos_targets zone_navigation_tests)
    add_executable(plaza_tests tests/plaza_tests.cpp)
    target_link_libraries(plaza_tests PRIVATE ecosplaza ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)
    add_dependencies(plaza_tests ecos_shaders)
    add_test(NAME plaza_assets_cpu COMMAND plaza_tests "${CMAKE_CURRENT_BINARY_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/assets/PLAZA_INSTANCES.csv" "${CMAKE_CURRENT_BINARY_DIR}/plaza-import.csv" "${CMAKE_CURRENT_BINARY_DIR}/plaza-scratch")
    set_tests_properties(plaza_assets_cpu PROPERTIES FIXTURES_SETUP plaza_import)
    add_executable(sdk_v1_tests tests/sdk_v1_tests.cpp tests/sdk_consumer/LessonModule.cpp)
    target_link_libraries(sdk_v1_tests PRIVATE ecoscontent ecoscore)
    add_dependencies(sdk_v1_tests ecos_shaders)
    add_test(NAME sdk_v1_cpu COMMAND sdk_v1_tests "${CMAKE_CURRENT_BINARY_DIR}/assets")
    add_test(NAME sdk_external_consumer COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk_external_test.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}/sdk-external"
        --core "$<TARGET_FILE:ecoscore>" --scene "$<TARGET_FILE:ecosscene>" --compiler "${CMAKE_CXX_COMPILER}"
        "--flags=${CMAKE_CXX_FLAGS}" "--link-flags=${CMAKE_EXE_LINKER_FLAGS}")
    list(APPEND ecos_targets sdk_v1_tests)
    add_test(NAME plaza_inventory COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_plaza_inventory.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --import-report "${CMAKE_CURRENT_BINARY_DIR}/plaza-import.csv"
        --usage-report "${CMAKE_CURRENT_SOURCE_DIR}/docs/evidence/phase5d/plaza/model-use.csv" --require-definitive --self-test)
    set_tests_properties(plaza_inventory PROPERTIES FIXTURES_REQUIRED plaza_import)
    add_test(NAME plaza_cli_rejects_module COMMAND ecosphere --plaza --module ecoenergy)
    set_tests_properties(plaza_cli_rejects_module PROPERTIES WILL_FAIL TRUE)
    list(APPEND ecos_targets plaza_tests)
    add_executable(ecoenergy_tests tests/ecoenergy_tests.cpp)
    target_link_libraries(ecoenergy_tests PRIVATE ecosplaza ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)
    add_dependencies(ecoenergy_tests ecos_shaders)
    add_test(NAME ecoenergy_assets_cpu COMMAND ecoenergy_tests "${CMAKE_CURRENT_BINARY_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/assets/ECOENERGY_INSTANCES.csv" "${CMAKE_CURRENT_BINARY_DIR}/ecoenergy-import.csv" "${CMAKE_CURRENT_BINARY_DIR}/ecoenergy-scratch")
    set_tests_properties(ecoenergy_assets_cpu PROPERTIES FIXTURES_SETUP ecoenergy_import)
    add_test(NAME ecoenergy_inventory COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_ecoenergy_inventory.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --import-report "${CMAKE_CURRENT_BINARY_DIR}/ecoenergy-import.csv"
        --usage-report "${CMAKE_CURRENT_SOURCE_DIR}/docs/evidence/phase6a/ecoenergy/model-use.csv" --require-definitive --self-test)
    set_tests_properties(ecoenergy_inventory PROPERTIES FIXTURES_REQUIRED ecoenergy_import)
    list(APPEND ecos_targets ecoenergy_tests)
    add_executable(aquabio_tests tests/aquabio_tests.cpp)
    target_link_libraries(aquabio_tests PRIVATE ecosplaza ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)
    add_dependencies(aquabio_tests ecos_shaders)
    add_test(NAME aquabio_assets_cpu COMMAND aquabio_tests "${CMAKE_CURRENT_BINARY_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/assets/AQUABIO_INSTANCES.csv" "${CMAKE_CURRENT_BINARY_DIR}/aquabio-import.csv" "${CMAKE_CURRENT_BINARY_DIR}/aquabio-scratch")
    set_tests_properties(aquabio_assets_cpu PROPERTIES FIXTURES_SETUP aquabio_import)
    add_test(NAME cli_rejects_aquabio_module COMMAND ecosphere --aquabio-smoke unused --module aquabio)
    add_test(NAME cli_rejects_two_installation_smokes COMMAND ecosphere --aquabio-smoke unused --ecoenergy-smoke unused)
    set_tests_properties(cli_rejects_aquabio_module cli_rejects_two_installation_smokes PROPERTIES WILL_FAIL TRUE)
    add_test(NAME aquabio_inventory COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_aquabio_inventory.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --import-report "${CMAKE_CURRENT_BINARY_DIR}/aquabio-import.csv"
        --usage-report "${CMAKE_CURRENT_SOURCE_DIR}/docs/evidence/aquabio-pond-animation/model-use.csv" --require-definitive --self-test)
    set_tests_properties(aquabio_inventory PROPERTIES FIXTURES_REQUIRED aquabio_import)
    add_test(NAME aquabio_spatial_layout COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_aquabio_space.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --bounds "${CMAKE_CURRENT_BINARY_DIR}/aquabio-space-bounds.csv"
        --views "${CMAKE_CURRENT_SOURCE_DIR}/docs/evidence/aquabio-pond-animation/views.csv"
        --report "${CMAKE_CURRENT_BINARY_DIR}/aquabio-space-audit.json" --self-test)
    set_tests_properties(aquabio_spatial_layout PROPERTIES FIXTURES_REQUIRED aquabio_import)
    list(APPEND ecos_targets aquabio_tests)
    add_executable(ecocycle_tests tests/ecocycle_tests.cpp)
    target_link_libraries(ecocycle_tests PRIVATE ecosplaza ecoscontent ecoscore EcoEnergy AquaBio EcoCycle)
    add_dependencies(ecocycle_tests ecos_shaders)
    add_test(NAME ecocycle_assets_cpu COMMAND ecocycle_tests "${CMAKE_CURRENT_BINARY_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/assets/ECOCYCLE_INSTANCES.csv" "${CMAKE_CURRENT_BINARY_DIR}/ecocycle-import.csv" "${CMAKE_CURRENT_BINARY_DIR}/ecocycle-scratch")
    set_tests_properties(ecocycle_assets_cpu PROPERTIES FIXTURES_SETUP ecocycle_import)
    add_test(NAME ecocycle_inventory COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_ecocycle_inventory.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --import-report "${CMAKE_CURRENT_BINARY_DIR}/ecocycle-import.csv" --self-test --require-definitive)
    add_test(NAME ecocycle_spatial_layout COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_ecocycle_space.py"
        --root "${CMAKE_CURRENT_SOURCE_DIR}" --bounds "${CMAKE_CURRENT_BINARY_DIR}/ecocycle-space-bounds.csv" --report "${CMAKE_CURRENT_BINARY_DIR}/ecocycle-space-audit.json" --self-test)
    set_tests_properties(ecocycle_inventory ecocycle_spatial_layout PROPERTIES FIXTURES_REQUIRED ecocycle_import)
    add_test(NAME cli_rejects_ecocycle_module COMMAND ecosphere --ecocycle-smoke unused --module ecocycle)
    add_test(NAME cli_rejects_ecocycle_mixed COMMAND ecosphere --ecocycle-smoke unused --aquabio-smoke unused)
    set_tests_properties(cli_rejects_ecocycle_module cli_rejects_ecocycle_mixed PROPERTIES WILL_FAIL TRUE)
    list(APPEND ecos_targets ecocycle_tests)
    add_executable(content_registry_tests tests/content_registry_tests.cpp)
    target_link_libraries(content_registry_tests PRIVATE ecoscontent ecoscore)
    add_test(NAME content_registry_cpu COMMAND content_registry_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/common" "${CMAKE_CURRENT_BINARY_DIR}/content-scratch")
    add_executable(content_module_tests tests/content_module_tests.cpp src/app/InteractionDemoInput.cpp)
    target_compile_definitions(content_module_tests PRIVATE GLFW_INCLUDE_NONE)
    target_link_libraries(content_module_tests PRIVATE ecoscontent ecoscore EcoEnergy AquaBio EcoCycle glfw)
    add_dependencies(content_module_tests ecos_shaders)
    add_test(NAME content_modules_integration_cpu COMMAND content_module_tests "${CMAKE_CURRENT_BINARY_DIR}/assets")
    list(APPEND ecos_targets content_registry_tests content_module_tests)
    add_executable(module_contract_tests tests/module_contract_tests.cpp)
    target_link_libraries(module_contract_tests PRIVATE ecoscore EcoEnergy AquaBio EcoCycle)
    add_test(NAME module_contract COMMAND module_contract_tests)
    add_executable(graphics_failure_tests tests/graphics_failure_tests.cpp)
    target_link_libraries(graphics_failure_tests PRIVATE ecosgraphics)
    add_test(NAME graphics_failure_cleanup COMMAND graphics_failure_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/shaders")
    add_executable(scene_camera_tests tests/scene_camera_tests.cpp)
    target_link_libraries(scene_camera_tests PRIVATE ecosscene)
    add_test(NAME scene_camera_cpu COMMAND scene_camera_tests)
    add_executable(asset_tests tests/asset_tests.cpp)
    target_link_libraries(asset_tests PRIVATE ecosassets)
    add_test(NAME assets_cpu COMMAND asset_tests "${CMAKE_CURRENT_BINARY_DIR}/asset-test-scratch" "${CMAKE_CURRENT_SOURCE_DIR}/assets/common/models")
    add_executable(lighting_tests tests/lighting_tests.cpp)
    target_link_libraries(lighting_tests PRIVATE ecosassets)
    add_test(NAME lighting_cpu COMMAND lighting_tests)
    add_executable(lighting_asset_tests tests/lighting_asset_tests.cpp)
    target_link_libraries(lighting_asset_tests PRIVATE ecosassets)
    add_test(NAME lighting_assets_cpu COMMAND lighting_asset_tests "${CMAKE_CURRENT_BINARY_DIR}/lighting-asset-scratch")
    add_executable(interaction_animation_tests tests/interaction_animation_tests.cpp)
    target_link_libraries(interaction_animation_tests PRIVATE ecosassets)
    add_test(NAME interaction_animation_cpu COMMAND interaction_animation_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/common")
    list(APPEND ecos_targets interaction_animation_tests)
    add_test(NAME cli_rejects_unknown_module COMMAND ecosphere --module missing)
    set_tests_properties(cli_rejects_unknown_module PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_rejects_unknown_option COMMAND ecosphere --unknown)
    set_tests_properties(cli_rejects_unknown_option PROPERTIES WILL_FAIL TRUE)
    list(APPEND ecos_targets module_contract_tests graphics_failure_tests scene_camera_tests asset_tests lighting_tests lighting_asset_tests)
endif()
foreach(target IN LISTS ecos_targets)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
