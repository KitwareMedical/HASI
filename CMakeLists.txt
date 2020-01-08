cmake_minimum_required(VERSION 3.10.2)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11) # Supported values are ``11``, ``14``, and ``17``.
endif()
if(NOT CMAKE_CXX_STANDARD_REQUIRED)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(NOT CMAKE_CXX_EXTENSIONS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

project(HASI CXX) # High-throughput Application for Skeletal Imaging

find_package(ITK 5 REQUIRED
  COMPONENTS
    ITKSmoothing
    ITKThresholding
    ITKConnectedComponents
    ITKDistanceMap
    ITKRegionGrowing
    ITKImageGrid
    ITKLabelMap
    ITKRegistrationCommon
    ITKIOTransformInsightLegacy
    ITKSpatialObjects
    BoneEnhancement
    ITKIOMeta
    ITKIONRRD
    )
include(${ITK_USE_FILE})

add_executable(segmentBonesInMicroCT segmentBonesInMicroCT.cxx)
add_executable(registerAndTransferLabels registerAndTransferLabels.cxx)

target_link_libraries(segmentBonesInMicroCT ${ITK_LIBRARIES})
target_link_libraries(registerAndTransferLabels ${ITK_LIBRARIES})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT segmentBonesInMicroCT)

set(INSTALL_RUNTIME_DESTINATION bin CACHE STRING "Install destination")

install(
  TARGETS
    segmentBonesInMicroCT
    registerAndTransferLabels
  DESTINATION ${INSTALL_RUNTIME_DESTINATION}
  )
