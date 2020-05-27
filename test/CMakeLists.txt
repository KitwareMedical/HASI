itk_module_test()

set(HASITests
  itkLandmarkAtlasSegmentationFilterTest.cxx
  itkSegmentBonesInMicroCTFilterTest.cxx
  )

CreateTestDriver(HASI "${HASI-Test_LIBRARIES}" "${HASITests}")

itk_add_test(NAME itkSegment901LTest
  COMMAND HASITestDriver
    --compare
    DATA{Baseline/901-L-label.nrrd}
    ${ITK_TEST_OUTPUT_DIR}/901-L-label.nrrd
  itkSegmentBonesInMicroCTFilterTest
    DATA{Input/901-L.nrrd}
    ${ITK_TEST_OUTPUT_DIR}/901-L-label.nrrd
  )

itk_add_test(NAME itkLandmarkAtlasSegmentationFilterTest
  COMMAND HASITestDriver
    --compare
    DATA{Baseline/907-L-A.nrrd}
    ${ITK_TEST_OUTPUT_DIR}/907-L-A.nrrd
  itkLandmarkAtlasSegmentationFilterTest
    DATA{Input/901-L.nrrd}
    DATA{Input/907-L.nrrd}
    ${ITK_TEST_OUTPUT_DIR}/901-L-label.nrrd
    DATA{Input/907-L-atlas.nrrd}
    DATA{Input/901-L.fcsv}
    DATA{Input/907-L.fcsv}
    ${ITK_TEST_OUTPUT_DIR}/907-L-
  )
set_tests_properties(itkLandmarkAtlasSegmentationFilterTest
    PROPERTIES DEPENDS itkSegment901LTest)
