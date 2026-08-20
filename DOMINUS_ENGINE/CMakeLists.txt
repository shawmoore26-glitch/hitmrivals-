cmake_minimum_required(VERSION 3.20)
project(DominusEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# --- CORE (Phase 1) -------------------------------------------------------
# Only CORE is built at v0.1. GRAPHICS/ANIMATION/CHARACTER/COMBAT/WORLD/AI
# directories exist as placeholders (see their README.md) and are added to
# this build as each phase gate opens -- see ROADMAP.md.

add_library(dominus_core STATIC
  CORE/Serialization/DominusSerializer.cpp
)
target_include_directories(dominus_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(dominus_core PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(dominus_core PUBLIC Threads::Threads)

# --- ANIMATION (Phase 2 + Motion Intelligence extension) -------------------
# Depends on CORE only for MiniJson + Result<T> -- never the reverse. See
# docs/ARCHITECTURE_v0.1.md section 3.
add_library(dominus_animation STATIC
  ANIMATION/SkeletonSystem/SkeletonLoader.cpp
  ANIMATION/SkeletonSystem/AnimationClipLoader.cpp
  ANIMATION/AnimationGraph/MotionGraphLoader.cpp
  ANIMATION/IK/IKChainLoader.cpp
  ANIMATION/Retargeting/RetargetMapLoader.cpp
)
target_link_libraries(dominus_animation PUBLIC dominus_core)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_animation PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- CHARACTER (Phase 2 + Genome extension) --------------------------------
# The composition layer: resolves CORE's generic refs into ANIMATION's real
# Skeleton/AnimationClip types on a MetaBinObject.
add_library(dominus_character STATIC
  CHARACTER/Rig/RigBinder.cpp
  CHARACTER/Genome/CombatIdentityLoader.cpp
  CHARACTER/Genome/SocialGenomeLoader.cpp
  CHARACTER/Genome/CreatureGenomeLoader.cpp
  CHARACTER/Genome/CombatStyleGenomeLoader.cpp
  CHARACTER/Genome/CombatPhysicsGenomeLoader.cpp
  CHARACTER/Genome/GameDesignGenomeLoader.cpp
  CHARACTER/Genome/VisualGenomeLoader.cpp
  CHARACTER/Genome/MaterialGenomeLoader.cpp
  CHARACTER/Genome/VisualStyleGenomeLoader.cpp
  CHARACTER/HitmBridge/HitmIdentityImporter.cpp
  CHARACTER/HitmBridge/HitmCombatGenome.cpp
  CHARACTER/HitmBridge/HitmPartsRig.cpp
  CHARACTER/HitmBridge/HitmGameRules.cpp
)
target_link_libraries(dominus_character PUBLIC dominus_animation)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_character PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- COMBAT (Phase 3) -------------------------------------------------------
# LAW C012: never a separate animation system -- COMBAT drives the SAME
# MotionGraphEvaluator built in ANIMATION/CHARACTER, so it links against
# dominus_character rather than reimplementing motion.
add_library(dominus_combat STATIC
  COMBAT/HitSystem/MoveLoader.cpp
  COMBAT/HitSystem/HurtboxLoader.cpp
  COMBAT/HitSystem/CombatBinder.cpp
  COMBAT/TransformationSystem.cpp
)
target_link_libraries(dominus_combat PUBLIC dominus_character)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_combat PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- WORLD (Phase 4.0 + Society Phase 0 persistence) -----------------------
# LAW: WORLD depends on CORE only -- never CHARACTER/COMBAT/ANIMATION.
# No longer header-only as of the persistence layer (WorldPersistence.cpp
# does real file I/O), so this is a real static library now.
add_library(dominus_world STATIC
  WORLD/Core/WorldPersistence.cpp
)
target_link_libraries(dominus_world PUBLIC dominus_core)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_world PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- PHYSICS (Phase 4.1) -----------------------------------------------------
# LAW: "WORLD knows physics exists. Physics does not know games exist."
# PHYSICS depends on WORLD (position lives in SpatialComponent) and CORE
# only -- never CHARACTER/COMBAT/ANIMATION. Header-only, same as WORLD.
add_library(dominus_physics INTERFACE)
target_link_libraries(dominus_physics INTERFACE dominus_world)

# --- HITM RUNTIME (Track H Module 5A) ---------------------------------------
# CHARACTER/HitmBridge/HitmMoveInstance.cpp and HitmFighterRuntime.cpp live
# under CHARACTER/HitmBridge/ alongside the other Track H bridge modules,
# but cannot be built into dominus_character itself: they depend on
# COMBAT::MoveDef/ReactionSystem and WORLD/PHYSICS, and dominus_combat
# already depends on dominus_character -- putting them in dominus_character
# would create a cycle (combat -> character -> combat). A new, small,
# separate library the same way dominus_rig/dominus_reality are their own
# libraries, exactly mirroring the "extension outside the core modules'
# mutual boundaries" pattern tests/world/test_hitm_rivals_as_world_entity.cpp
# already established.
#
# Track H Module 5B (real sprite/texture integration) adds its four files
# to this SAME library rather than a new one: they depend on
# HitmFighterRuntime's public HitmFighterSnapshot/HitmFighterState (this
# library) plus HitmPartsRig/HitmIdentityRecord (dominus_character,
# already reachable transitively through dominus_combat below) -- no new
# cross-module dependency, no new library needed.
add_library(dominus_hitm_runtime STATIC
  CHARACTER/HitmBridge/HitmMoveInstance.cpp
  CHARACTER/HitmBridge/HitmFighterRuntime.cpp
  CHARACTER/HitmBridge/HitmAnimationSet.cpp
  CHARACTER/HitmBridge/HitmRigPlacement.cpp
  CHARACTER/HitmBridge/HitmAssetImporter.cpp
  CHARACTER/HitmBridge/HitmSpriteDrawData.cpp
  CHARACTER/HitmBridge/HitmMeleeHitCheck.cpp
  CHARACTER/HitmBridge/HitmRushAttack.cpp
  CHARACTER/HitmBridge/HitmMatch.cpp
)
target_link_libraries(dominus_hitm_runtime PUBLIC dominus_combat dominus_world dominus_physics)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_hitm_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- REGISTRY (Phase 1 prototype: hash + immutable artifacts) --------------
# Scoped to CombatGenome only, per the prototype directive. Depends on
# CHARACTER/Genome only -- never COMBAT/ANIMATION/WORLD/PHYSICS, verified
# by grep before any test was written. Header-only.
add_library(dominus_registry INTERFACE)
target_link_libraries(dominus_registry INTERFACE dominus_character)

# --- VISUALFORGE (bridge: reality description -> production spec) ---------
# Depends on CHARACTER/Genome (VisualGenome/MaterialGenome/
# VisualStyleGenome/VisualMemorySummary) and, transitively via
# VisualMemorySummary, WORLD -- never GRAPHICS (an ungated placeholder,
# see GRAPHICS/README.md and VISUALFORGE/README.md for why Visual Forge
# does not live there). Header-only, same as REGISTRY.
# --- GRAPHICS (real, minimal, gate opened this phase -- see
# GRAPHICS/README.md for the honest scope: a deterministic logical
# frame compiler, plus (as of the Raster phase) a real rasterizer that
# turns a Frame into real pixels -- NOT a mesh/asset renderer, see
# GRAPHICS/Raster/RasterDevice.h for exactly what it does and doesn't
# claim). Depends on ANIMATION::Transform2D and REGISTRY::Hash only --
# zero knowledge of VISUALFORGE's types, dependency direction is
# strictly VISUALFORGE -> GRAPHICS, never the reverse. Now a real
# static library (RasterDevice.cpp is real, compiled code), not
# header-only.
#
# Texture Capability phase (Track H Phase 5A): GRAPHICS/Raster/
# PngDecoder.cpp is a real PNG decoder (see its own header comment for
# exact scope) that uses the system's real zlib for DEFLATE
# decompression -- the one real, new, disclosed external dependency
# this phase adds to the CPU test suite (unlike DOMINUS_ENABLE_VULKAN
# below, this is NOT gated -- zlib is a real, standard, near-universally
# available system library, not an optional GPU SDK).
find_package(ZLIB REQUIRED)
add_library(dominus_graphics STATIC
  GRAPHICS/Raster/RasterDevice.cpp
  GRAPHICS/Raster/PngDecoder.cpp
  GRAPHICS/Renderer/SceneFromEntities.cpp
  GRAPHICS/Renderer/MeshLibrary.cpp
  GRAPHICS/Renderer/MaterialContract.cpp
)
target_link_libraries(dominus_graphics PUBLIC dominus_animation dominus_registry dominus_world dominus_character ZLIB::ZLIB)

# --- GRAPHICS/Vulkan (real GPU renderer, gated -- OFF by default) --------
# The first real GPU backend: Frame -> CPU vertex staging -> Vulkan
# vertex buffer -> real GPU rasterization -> swapchain -> presentation.
# See GRAPHICS/Vulkan/README.md for exact scope (not a claim that mesh/
# texture/material asset resolution exists -- every DrawCommand becomes
# a real GPU-rendered rectangle from the same Frame RasterDevice
# already visualizes on the CPU). Defaults OFF so the established CPU
# test suite -- everything this engine has proven across every prior
# phase -- remains buildable with zero new dependencies. When
# explicitly enabled, a missing Vulkan/GLFW/glslc is a real CMake
# configuration error, never a silent fallback to fake success.
option(DOMINUS_ENABLE_VULKAN "Build the real Vulkan GPU renderer" OFF)
if(DOMINUS_ENABLE_VULKAN)
  find_package(Vulkan REQUIRED)
  find_package(glfw3 CONFIG QUIET)
  if(TARGET glfw)
    set(DOMINUS_GLFW_TARGET glfw)
  elseif(TARGET glfw3)
    set(DOMINUS_GLFW_TARGET glfw3)
  else()
    message(FATAL_ERROR "DOMINUS_ENABLE_VULKAN requires GLFW 3 with a CMake target (glfw or glfw3)")
  endif()

  find_program(DOMINUS_GLSLC glslc REQUIRED)
  set(DOMINUS_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/GRAPHICS/Shaders)
  file(MAKE_DIRECTORY ${DOMINUS_SHADER_DIR})

  add_custom_command(
    OUTPUT ${DOMINUS_SHADER_DIR}/dominus_triangle.vert.spv
    COMMAND ${DOMINUS_GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_triangle.vert -o ${DOMINUS_SHADER_DIR}/dominus_triangle.vert.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_triangle.vert
    VERBATIM)
  add_custom_command(
    OUTPUT ${DOMINUS_SHADER_DIR}/dominus_triangle.frag.spv
    COMMAND ${DOMINUS_GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_triangle.frag -o ${DOMINUS_SHADER_DIR}/dominus_triangle.frag.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_triangle.frag
    VERBATIM)
  add_custom_command(
    OUTPUT ${DOMINUS_SHADER_DIR}/dominus_material_resource.vert.spv
    COMMAND ${DOMINUS_GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_material_resource.vert -o ${DOMINUS_SHADER_DIR}/dominus_material_resource.vert.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_material_resource.vert
    VERBATIM)
  add_custom_command(
    OUTPUT ${DOMINUS_SHADER_DIR}/dominus_material_resource.frag.spv
    COMMAND ${DOMINUS_GLSLC} ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_material_resource.frag -o ${DOMINUS_SHADER_DIR}/dominus_material_resource.frag.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/GRAPHICS/Shaders/dominus_material_resource.frag
    VERBATIM)
  add_custom_target(dominus_gpu_shaders DEPENDS
    ${DOMINUS_SHADER_DIR}/dominus_triangle.vert.spv
    ${DOMINUS_SHADER_DIR}/dominus_triangle.frag.spv
    ${DOMINUS_SHADER_DIR}/dominus_material_resource.vert.spv
    ${DOMINUS_SHADER_DIR}/dominus_material_resource.frag.spv)

  target_sources(dominus_graphics PRIVATE GRAPHICS/Vulkan/VulkanFrameRenderer.cpp)
  add_dependencies(dominus_graphics dominus_gpu_shaders)
  target_link_libraries(dominus_graphics PUBLIC Vulkan::Vulkan ${DOMINUS_GLFW_TARGET})
  target_compile_definitions(dominus_graphics PRIVATE
    DOMINUS_GPU_SHADER_DIR=\"${DOMINUS_SHADER_DIR}\")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_graphics PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(dominus_visualforge INTERFACE)
target_link_libraries(dominus_visualforge INTERFACE dominus_character dominus_world dominus_registry dominus_graphics)

# --- RIG (DOMINUS RIG v1.0: canonical skeleton contract + validator) ------
# Depends on ANIMATION::Skeleton/RetargetMap for the contract/validator
# itself, and on COMBAT/CHARACTER/REGISTRY for CharacterAcceptanceHarness
# (RIG/AcceptanceCertificate.h), which is a genuine terminal consumer --
# it needs RigBinder, CombatBinder, CollisionEvaluator, CombatController,
# and Sha256 hashing all together to actually run a real acceptance
# check, the same "depends on everything, nothing depends on it" shape
# VALIDATION already has. Strict downward chain
# (RIG -> COMBAT -> CHARACTER -> ANIMATION -> CORE), no cycle.
# RigProfileLoader needs a .cpp (JSON parsing, mirroring
# RetargetMapLoader's own pattern), so this is STATIC.
add_library(dominus_rig STATIC
  RIG/RigProfileLoader.cpp
)
target_include_directories(dominus_rig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dominus_rig PUBLIC dominus_animation dominus_combat dominus_registry)

# --- VALIDATION (Phase 7) ---------------------------------------------------
# Terminal consumer -- depends on everything (CORE/ANIMATION/CHARACTER/
# COMBAT), nothing depends on it. Not header-only: needs RigBinder/
# CombatBinder/DominusSerializer calls, kept in a .cpp like the other
# binders.
add_library(dominus_validation STATIC
  VALIDATION/PackageValidator.cpp
  VALIDATION/BuildPipeline.cpp
)
target_link_libraries(dominus_validation PUBLIC dominus_combat)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_validation PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- REALITY (Reality Compiler: the first governed orchestration layer) ---
# Terminal consumer, same shape as VALIDATION -- depends on RIG and
# VISUALFORGE (both already terminal consumers of everything below
# them), nothing depends on it. Coordinates CharacterAcceptanceHarness
# and RendererPackageAcceptanceHarness without reimplementing either;
# see REALITY/RealityCompiler.h for the exact scope (Brooklyn only,
# Milestone 1 of the Reality Pipeline directive).
add_library(dominus_reality STATIC
  REALITY/RealityCompiler.cpp
  REALITY/BrooklynDomainCompilers.cpp
  REALITY/RealityRegistry.cpp
  REALITY/FileLock.cpp
  REALITY/RealityRebuilder.cpp
  REALITY/BrooklynEvidenceGraphBuilder.cpp
  REALITY/ChangeEventNormalizer.cpp
  REALITY/Reconciler.cpp
  REALITY/RealityWatcher.cpp
)
target_link_libraries(dominus_reality PUBLIC dominus_rig dominus_visualforge)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(dominus_reality PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tools -----------------------------------------------------------------
add_executable(dominus-cli TOOLS/Editor/dominus_cli.cpp)
target_link_libraries(dominus-cli PRIVATE dominus_combat dominus_world dominus_physics dominus_hitm_runtime dominus_registry dominus_visualforge dominus_rig dominus_validation dominus_reality)

if(DOMINUS_ENABLE_VULKAN)
  add_executable(dominus-gpu-demo TOOLS/Editor/dominus_gpu_demo.cpp)
  target_link_libraries(dominus-gpu-demo PRIVATE dominus_graphics)

  add_executable(dominus-gpu-scene-test TOOLS/Editor/dominus_gpu_scene_test.cpp)
  target_link_libraries(dominus-gpu-scene-test PRIVATE dominus_graphics dominus_character dominus_world)
  target_compile_definitions(dominus-gpu-scene-test PRIVATE
    DOMINUS_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\")

  add_executable(dominus-gpu-lifecycle-test TOOLS/Editor/dominus_gpu_lifecycle_test.cpp)
  target_link_libraries(dominus-gpu-lifecycle-test PRIVATE dominus_graphics)

  add_executable(dominus-gpu-material-resource-test TOOLS/Editor/dominus_gpu_material_resource_test.cpp)
  target_link_libraries(dominus-gpu-material-resource-test PRIVATE dominus_graphics dominus_character)
endif()

# --- Tests -----------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
