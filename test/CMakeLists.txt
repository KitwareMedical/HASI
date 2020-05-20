itk_module_test()

set(HASITests
  itkMinimalStandardRandomVariateGeneratorTest.cxx
  itkSegmentBonesInMicroCTFilterTest.cxx
  )

CreateTestDriver(HASI "${HASI-Test_LIBRARIES}" "${HASITests}")

itk_add_test(NAME itkMinimalStandardRandomVariateGeneratorTest
  COMMAND HASITestDriver itkMinimalStandardRandomVariateGeneratorTest
  )

itk_add_test(NAME itkSegmentBonesInMicroCTFilterTest
  COMMAND HASITestDriver
    --compare
    ${ITK_TEST_OUTPUT_DIR}/itkSegmentBonesInMicroCTFilterTestOutput.mha
    DATA{Baseline/itkSegmentBonesInMicroCTFilterTestOutput.mha}
  itkSegmentBonesInMicroCTFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkSegmentBonesInMicroCTFilterTestOutput.mha
  )
