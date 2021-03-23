# the top-level README is used for describing this module, just
# re-used it for documentation here
get_filename_component(MY_CURRENT_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
file(READ "${MY_CURRENT_DIR}/README.md" DOCUMENTATION)


itk_module(HASI
  DEPENDS
    ITKSmoothing
    ITKThresholding
    ITKConnectedComponents
    ITKDistanceMap
    ITKRegionGrowing
    ITKImageGrid
    ITKLabelMap
    ITKRegistrationCommon
    ITKSpatialObjects
    ITKTransform
    BoneEnhancement
  COMPILE_DEPENDS
    ITKImageSources
  TEST_DEPENDS
    ITKTestKernel
    ITKMetaIO
    ITKIONRRD
    ITKIOTransformInsightLegacy
    ITKIOTransformHDF5
  DESCRIPTION
    "${DOCUMENTATION}"
  EXCLUDE_FROM_DEFAULT
  ENABLE_SHARED
)
