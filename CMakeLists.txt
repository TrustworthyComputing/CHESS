cmake_minimum_required(VERSION 3.16)

project(SchemeSwitching VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_STATIC "Link against the static OpenFHE libraries" OFF)
option(BUILD_EXAMPLES "Build executables under examples/" ON)
option(BUILD_MLIR_COMPILER "Configure the MLIR-based compiler in src/" OFF)

set(_scheme_switching_prefix_hints
    "$ENV{OpenFHE_ROOT}"
    "$ENV{LLVM_ROOT}"
    "$ENV{MLIR_ROOT}"
    "/opt/openfhe"
    "/opt/llvm"
    "/usr/local"
    "/usr"
)
set(_scheme_switching_prefix_path "${CMAKE_PREFIX_PATH}")
foreach(prefix IN LISTS _scheme_switching_prefix_hints)
    if(prefix AND IS_DIRECTORY "${prefix}")
        list(APPEND _scheme_switching_prefix_path "${prefix}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _scheme_switching_prefix_path)
set(CMAKE_PREFIX_PATH "${_scheme_switching_prefix_path}")

if(NOT OpenFHE_DIR AND DEFINED ENV{OpenFHE_DIR})
    set(OpenFHE_DIR "$ENV{OpenFHE_DIR}" CACHE PATH "OpenFHE CMake package directory")
endif()
foreach(config_dir IN ITEMS
    "/usr/local/lib/OpenFHE"
    "/usr/lib/OpenFHE"
    "/opt/openfhe/lib/OpenFHE"
    "/opt/openfhe/lib/cmake/OpenFHE"
)
    if(NOT OpenFHE_DIR AND EXISTS "${config_dir}/OpenFHEConfig.cmake")
        set(OpenFHE_DIR "${config_dir}" CACHE PATH "OpenFHE CMake package directory")
    endif()
endforeach()

find_package(OpenFHE REQUIRED CONFIG)
find_package(OpenMP)

message(STATUS "Found OpenFHE ${BASE_OPENFHE_VERSION}")
message(STATUS "OpenFHE include files located in: ${OpenFHE_INCLUDE}")
message(STATUS "OpenFHE libraries located in: ${OpenFHE_LIBDIR}")

set(OpenFHE_INCLUDE_DIRS
    ${OpenFHE_INCLUDE}
    ${OpenFHE_INCLUDE}/third-party/include
    ${OpenFHE_INCLUDE}/core
    ${OpenFHE_INCLUDE}/pke
    ${OpenFHE_INCLUDE}/binfhe
)

set(_scheme_switching_sources
    src/backend/cggi_operations.cpp
    src/backend/ckks_operations.cpp
)
foreach(_optional_src IN ITEMS test/output.cpp test/test.cpp)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_optional_src}")
        list(APPEND _scheme_switching_sources "${_optional_src}")
    else()
        message(STATUS "Skipping missing source: ${_optional_src}")
    endif()
endforeach()

if(_scheme_switching_sources)
    add_library(scheme_switching STATIC ${_scheme_switching_sources})
    target_include_directories(scheme_switching
        SYSTEM PUBLIC
            ${OpenFHE_INCLUDE_DIRS}
    )
    if(OpenFHE_CXX_FLAGS)
        separate_arguments(_openfhe_cxx_flags NATIVE_COMMAND "${OpenFHE_CXX_FLAGS}")
        target_compile_options(scheme_switching PRIVATE ${_openfhe_cxx_flags})
    endif()
else()
    message(WARNING "No sources available for scheme_switching; skipping library build.")
endif()

if(BUILD_STATIC AND OpenFHE_STATIC_LIBRARIES)
    set(_scheme_switching_openfhe_libs ${OpenFHE_STATIC_LIBRARIES})
else()
    set(_scheme_switching_openfhe_libs ${OpenFHE_SHARED_LIBRARIES})
endif()
if(NOT _scheme_switching_openfhe_libs AND DEFINED OpenFHE_LIBRARIES)
    set(_scheme_switching_openfhe_libs ${OpenFHE_LIBRARIES})
endif()
if(TARGET scheme_switching)
    target_link_libraries(scheme_switching PUBLIC ${_scheme_switching_openfhe_libs})
endif()

set(_scheme_switching_link_flags "")
if(OpenFHE_EXE_LINKER_FLAGS)
    separate_arguments(_openfhe_link_flags NATIVE_COMMAND "${OpenFHE_EXE_LINKER_FLAGS}")
    list(APPEND _scheme_switching_link_flags ${_openfhe_link_flags})
endif()
if(BUILD_STATIC)
    list(APPEND _scheme_switching_link_flags -static)
endif()
set(_scheme_switching_exe_link_flags ${_scheme_switching_link_flags})

set(RUN_OUTPUT_CPP "" CACHE FILEPATH "Path to a generated C++ file to build as run_output")
if(RUN_OUTPUT_CPP)
    if(NOT EXISTS "${RUN_OUTPUT_CPP}")
        message(FATAL_ERROR "RUN_OUTPUT_CPP not found: ${RUN_OUTPUT_CPP}")
    endif()
    if(NOT TARGET scheme_switching)
        message(FATAL_ERROR "RUN_OUTPUT_CPP requires scheme_switching, but the library is not available.")
    endif()
    add_executable(run_output "${RUN_OUTPUT_CPP}")
    target_link_libraries(run_output PRIVATE scheme_switching)
    if(_scheme_switching_exe_link_flags)
        target_link_options(run_output PRIVATE ${_scheme_switching_exe_link_flags})
    endif()
endif()

if(BUILD_EXAMPLES AND TARGET scheme_switching)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/main.cpp")
        add_executable(main examples/main.cpp)
        target_link_libraries(main PRIVATE scheme_switching)
        if(_scheme_switching_exe_link_flags)
            target_link_options(main PRIVATE ${_scheme_switching_exe_link_flags})
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/example.cpp")
        add_executable(example examples/example.cpp)
        target_link_libraries(example PRIVATE scheme_switching)
        if(_scheme_switching_exe_link_flags)
            target_link_options(example PRIVATE ${_scheme_switching_exe_link_flags})
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/ir.cpp")
        add_executable(ir examples/ir.cpp)
        target_link_libraries(ir PRIVATE scheme_switching)
        if(_scheme_switching_exe_link_flags)
            target_link_options(ir PRIVATE ${_scheme_switching_exe_link_flags})
        endif()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/experiments.cpp")
        add_executable(experiments examples/experiments.cpp)
        target_link_libraries(experiments PRIVATE scheme_switching)
        if(OpenMP_CXX_FOUND)
            target_link_libraries(experiments PRIVATE OpenMP::OpenMP_CXX)
        endif()
        if(_scheme_switching_exe_link_flags)
            target_link_options(experiments PRIVATE ${_scheme_switching_exe_link_flags})
        endif()
    endif()
endif()

if(BUILD_MLIR_COMPILER)
    # CMP0116: Ninja generators transform `DEPFILE`s from `add_custom_command()`
    if(POLICY CMP0116)
        cmake_policy(SET CMP0116 OLD)
    endif()

    if(NOT MLIR_DIR AND DEFINED ENV{MLIR_DIR})
        set(MLIR_DIR "$ENV{MLIR_DIR}" CACHE PATH "MLIR CMake package directory")
    endif()
    if(NOT LLVM_DIR AND DEFINED ENV{LLVM_DIR})
        set(LLVM_DIR "$ENV{LLVM_DIR}" CACHE PATH "LLVM CMake package directory")
    endif()
    file(GLOB _scheme_switching_llvm_cmake_dirs "/usr/lib/llvm-*/lib/cmake")
    foreach(cmake_dir IN LISTS _scheme_switching_llvm_cmake_dirs)
        if(NOT MLIR_DIR AND EXISTS "${cmake_dir}/mlir/MLIRConfig.cmake")
            set(MLIR_DIR "${cmake_dir}/mlir" CACHE PATH "MLIR CMake package directory")
        endif()
        if(NOT LLVM_DIR AND EXISTS "${cmake_dir}/llvm/LLVMConfig.cmake")
            set(LLVM_DIR "${cmake_dir}/llvm" CACHE PATH "LLVM CMake package directory")
        endif()
    endforeach()

    if(NOT MLIR_DIR AND EXISTS "/opt/llvm/lib/cmake/mlir/MLIRConfig.cmake")
        set(MLIR_DIR "/opt/llvm/lib/cmake/mlir" CACHE PATH "MLIR CMake package directory")
    endif()
    if(NOT LLVM_DIR AND EXISTS "/opt/llvm/lib/cmake/llvm/LLVMConfig.cmake")
        set(LLVM_DIR "/opt/llvm/lib/cmake/llvm" CACHE PATH "LLVM CMake package directory")
    endif()

    find_package(MLIR REQUIRED CONFIG)

    message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

    set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR})
    set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR})
    set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})

    list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
    include(TableGen)
    include(AddLLVM)
    include(AddMLIR)
    include(HandleLLVMOptions)

    if(LLVM_BUILD_LIBRARY_DIR)
        link_directories(${LLVM_BUILD_LIBRARY_DIR})
    endif()

    set(LLVM_LIT_ARGS "-sv" CACHE STRING "lit default options")
    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

    get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
    get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
    set(MLIR_COMPILER_LIBS
        ${dialect_libs}
        ${conversion_libs}
        MLIREmitCDialect
        MLIROptLib
        MLIRAnalysis
        MLIRCallInterfaces
        MLIRCastInterfaces
        MLIRExecutionEngine
        MLIRIR
        MLIRLLVMCommonConversion
        MLIRLLVMToLLVMIRTranslation
        MLIRMemRefDialect
        MLIRParser
        MLIRPass
        MLIRInferTypeOpInterface
        MLIRControlFlowInterfaces
        MLIRSideEffectInterfaces
        MLIRTargetLLVMIRExport
        MLIRTargetCpp
        MLIRTransforms
    )
    add_llvm_executable(chess
        src/frontend/compiler.cpp
        src/frontend/main_emitter.cpp
    )

    llvm_update_compile_flags(chess)
    target_include_directories(chess
        PRIVATE
            ${LLVM_INCLUDE_DIRS}
            ${MLIR_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend
    )
    if(LLVM_DEFINITIONS)
        target_compile_options(chess PRIVATE ${LLVM_DEFINITIONS})
    endif()
    target_link_libraries(chess PRIVATE ${MLIR_COMPILER_LIBS})
    set_target_properties(chess PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
