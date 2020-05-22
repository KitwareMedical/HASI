/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "itkLandmarkAtlasSegmentationFilter.h"

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

int itkLandmarkAtlasSegmentationFilterTest(int argc, char * argv[])
{
  if (argc < 7)
  {
    std::cerr << "Missing parameters." << std::endl;
    std::cerr << "Usage: " << itkNameOfTestExecutableMacro(argv);
    std::cerr << " <inputImage> <atlasImage> <atlasLabels> <inputLandmarks> <atlasLandmarks>";
    std::cerr << " <outputLabels> [affineTransform] [bsplineTransform]";
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }
  const char * inputImage = argv[1];
  const char * atlasImage = argv[2];
  const char * atlasLabels = argv[3];
  const char * inputLandmarks = argv[4];
  const char * atlasLandmarks = argv[5];
  const char * outputLabels = argv[6];

  float corticalThickness = 0.1;
  if (argc > 3)
  {
    corticalThickness = std::stof(argv[3]);
  }

  constexpr unsigned int Dimension = 3;
  using PixelType = short;
  using ImageType = itk::Image<PixelType, Dimension>;

  using FilterType = itk::LandmarkAtlasSegmentationFilter<ImageType, ImageType>;
  FilterType::Pointer filter = FilterType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(filter, LandmarkAtlasSegmentationFilter, ImageToImageFilter);

  std::cout << "Reading image: " << inputImage << std::endl;
  using ReaderType = itk::ImageFileReader<ImageType>;
  typename ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(inputImage);
  ITK_TRY_EXPECT_NO_EXCEPTION(reader->Update());
  ImageType::Pointer image = reader->GetOutput();

  std::cout << "Running the filter" << std::endl;
  ShowProgress::Pointer showProgress = ShowProgress::New();
  filter->AddObserver(itk::ProgressEvent(), showProgress);
  filter->SetInput(image);
  filter->SetCorticalBoneThickness(corticalThickness);
  ITK_TRY_EXPECT_NO_EXCEPTION(filter->Update());

  std::cout << "Writing label map: " << outputLabels << std::endl;
  using WriterType = itk::ImageFileWriter<ImageType>;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outputLabels);
  writer->SetInput(filter->GetOutput());
  writer->SetUseCompression(true);
  ITK_TRY_EXPECT_NO_EXCEPTION(writer->Update());

  std::cout << "Test finished successfully." << std::endl;
  return EXIT_SUCCESS;
}
