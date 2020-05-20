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
    DATA{Baseline/itkLandmarkAtlasSegmentationFilterTestOutput.mha}
    ${ITK_TEST_OUTPUT_DIR}/itkLandmarkAtlasSegmentationFilterTestOutput.mha
  itkLandmarkAtlasSegmentationFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkLandmarkAtlasSegmentationFilterTestOutput.mha
  )
