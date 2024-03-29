/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         https://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "itkSegmentBonesInMicroCTFilter.h"

#include "itkCommand.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkTestingMacros.h"

namespace
{
class ShowProgress : public itk::Command
{
public:
  itkNewMacro(ShowProgress);

  void
  Execute(itk::Object * caller, const itk::EventObject & event) override
  {
    Execute((const itk::Object *)caller, event);
  }

  void
  Execute(const itk::Object * caller, const itk::EventObject & event) override
  {
    if (!itk::ProgressEvent().CheckEvent(&event))
    {
      return;
    }
    const auto * processObject = dynamic_cast<const itk::ProcessObject *>(caller);
    if (!processObject)
    {
      return;
    }
    std::cout << " " << processObject->GetProgress();
  }
};
} // namespace

int
itkSegmentBonesInMicroCTFilterTest(int argc, char * argv[])
{
  if (argc < 3)
  {
    std::cerr << "Missing parameters." << std::endl;
    std::cerr << "Usage: " << itkNameOfTestExecutableMacro(argv);
    std::cerr << " <inputImage> <outputImage> [corticalThickness] [wholeBones]";
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }
  const char * inputImageFileName = argv[1];
  const char * outputImageFileName = argv[2];

  float corticalThickness = 0.1;
  if (argc > 3)
  {
    corticalThickness = std::stof(argv[3]);
  }

  bool wholeBones = false;
  if (argc > 4)
  {
    wholeBones = std::stoi(argv[4]);
  }

  constexpr unsigned int Dimension = 3;
  using PixelType = short;
  using ImageType = itk::Image<PixelType, Dimension>;

  using FilterType = itk::SegmentBonesInMicroCTFilter<ImageType, ImageType>;
  FilterType::Pointer filter = FilterType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(filter, SegmentBonesInMicroCTFilter, ImageToImageFilter);

  std::cout << "Reading image: " << inputImageFileName << std::endl;
  ImageType::Pointer image;
  ITK_TRY_EXPECT_NO_EXCEPTION(image = itk::ReadImage<ImageType>(inputImageFileName));

  std::cout << "Running the filter" << std::endl;
  ShowProgress::Pointer showProgress = ShowProgress::New();
  filter->AddObserver(itk::ProgressEvent(), showProgress);
  filter->SetInput(image);
  filter->SetCorticalBoneThickness(corticalThickness);
  filter->SetWholeBones(wholeBones);
  ITK_TRY_EXPECT_NO_EXCEPTION(filter->Update());

  std::cout << "Writing label map: " << outputImageFileName << std::endl;
  ITK_TRY_EXPECT_NO_EXCEPTION(itk::WriteImage(filter->GetOutput(), outputImageFileName, true));

  std::cout << "Test finished successfully." << std::endl;
  return EXIT_SUCCESS;
}
