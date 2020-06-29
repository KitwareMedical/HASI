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
#include "itkBoneMorphometryFeaturesFilter.h"

#include "itkMath.h"
#include "itkImage.h"
#include "itkVector.h"
#include "itkImageFileReader.h"
#include "itkBinaryThresholdImageFilter.h"

int
main(int argc, char * argv[])
{
  if (argc < 3)
  {
    std::cerr << "Missing parameters." << std::endl;
    std::cerr << "Usage: " << argv[0] << " inputImageFile"
              << " maskImageFile [labelNumber]" << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    constexpr unsigned int ImageDimension = 3;

    using InputImageType = itk::Image<short, ImageDimension>;
    using ReaderType = itk::ImageFileReader<InputImageType>;
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(argv[1]);

    using MaskImageType = itk::Image<unsigned char, ImageDimension>;
    using MaskReaderType = itk::ImageFileReader<MaskImageType>;
    MaskReaderType::Pointer maskReader = MaskReaderType::New();
    maskReader->SetFileName(argv[2]);

    unsigned char labelNumber = 4;
    if (argc > 3)
    {
      labelNumber = std::stoul(argv[3]);
    }
    using BinaryThresholdType = itk::BinaryThresholdImageFilter<MaskImageType, MaskImageType>;
    typename BinaryThresholdType::Pointer binTh = BinaryThresholdType::New();
    binTh->SetInput(maskReader->GetOutput());
    binTh->SetLowerThreshold(labelNumber);
    binTh->SetUpperThreshold(labelNumber);

    using FilterType = itk::BoneMorphometryFeaturesFilter<InputImageType>;
    FilterType::Pointer filter = FilterType::New();
    filter->SetInput(reader->GetOutput());
    filter->SetMaskImage(binTh->GetOutput());
    filter->Update();

    std::cout << argv[1] << ", " << argv[2];
    std::cout << ", " << filter->GetBVTV() << ", " << filter->GetTbN() << ", " << filter->GetTbTh();
    std::cout << ", " << filter->GetTbSp() << ", " << filter->GetBSBV() << std::endl;

    return EXIT_SUCCESS;
  }
  catch (itk::ExceptionObject & exc)
  {
    std::cerr << exc;
  }
  catch (std::exception & exc)
  {
    std::cerr << exc.what();
  }
  catch (...)
  {
    std::cerr << "Unknown error has occurred" << std::endl;
  }
  return EXIT_FAILURE;
}
