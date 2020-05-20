itk_module_test()

set(HASITests
  itkLandmarkAtlasSegmentationFilterTest.cxx
  itkSegmentBonesInMicroCTFilterTest.cxx
  )

CreateTestDriver(HASI "${HASI-Test_LIBRARIES}" "${HASITests}")

itk_add_test(NAME itkLandmarkAtlasSegmentationFilterTest
  COMMAND HASITestDriver
    --compare
    ${ITK_TEST_OUTPUT_DIR}/itkLandmarkAtlasSegmentationFilterTestOutput.mha
    DATA{Baseline/itkLandmarkAtlasSegmentationFilterTestOutput.mha}
  itkLandmarkAtlasSegmentationFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkLandmarkAtlasSegmentationFilterTestOutput.mha
  )

itk_add_test(NAME itkSegmentBonesInMicroCTFilterTest
  COMMAND HASITestDriver
    --compare
    ${ITK_TEST_OUTPUT_DIR}/itkSegmentBonesInMicroCTFilterTestOutput.mha
    DATA{Baseline/itkSegmentBonesInMicroCTFilterTestOutput.mha}
  itkSegmentBonesInMicroCTFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkSegmentBonesInMicroCTFilterTestOutput.mha
  )
