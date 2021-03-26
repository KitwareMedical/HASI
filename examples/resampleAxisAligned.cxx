#include <chrono>
#include <iostream>

#include "itkImageFileReader.h"
#include "itkTransformFileReader.h"
#include "itkImageMaskSpatialObject.h"
#include "itkVersorRigid3DTransform.h"
#include "itkResampleImageFilter.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkRegionOfInterestImageFilter.h"
#include "itkImageFileWriter.h"

auto startTime = std::chrono::steady_clock::now();

template <typename TransformType>
itk::SmartPointer<TransformType>
ReadTransform(std::string fileName)
{
  using TransformReaderType = itk::TransformFileReaderTemplate<typename TransformType::ParametersValueType>;
  typename TransformReaderType::Pointer transformReader = TransformReaderType::New();
  transformReader->SetFileName(fileName);
  transformReader->Update();
  itk::Object * transformPointer = *(transformReader->GetModifiableTransformList()->begin());
  auto          transform = dynamic_cast<TransformType *>(transformPointer);
  return transform;
}

template <typename ImageType>
itk::SmartPointer<ImageType>
ResampleAxisAligned(itk::SmartPointer<ImageType>  input,
                    typename ImageType::PixelType defaultValue,
                    typename ImageType::PointType newOrigin,
                    typename ImageType::SizeType  newSize,
                    bool                          nearestNeighbor)
{
  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Resampling the image" << std::endl;
  using InterpolatorType = itk::NearestNeighborInterpolateImageFunction<ImageType, itk::SpacePrecisionType>;
  typename InterpolatorType::Pointer nnInterpolator = InterpolatorType::New();
  using ResampleFilterType = itk::ResampleImageFilter<ImageType, ImageType>;
  typename ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();
  resampleFilter->SetInput(input);
  resampleFilter->SetDefaultPixelValue(defaultValue);
  resampleFilter->SetOutputOrigin(newOrigin);
  resampleFilter->SetSize(newSize);
  resampleFilter->SetOutputSpacing(input->GetSpacing());
  if (nearestNeighbor)
  {
    resampleFilter->SetInterpolator(nnInterpolator);
  }
  resampleFilter->Update();
  return resampleFilter->GetOutput();
}

template <typename ImageType>
void
mainProcessing(std::string inputImage,
               std::string inputLabels,
               std::string transformFilename,
               std::string outputImage,
               std::string outputLabels)
{
  constexpr unsigned Dimension = ImageType::ImageDimension;
  using LabelImageType = itk::Image<unsigned char, Dimension>;
  using SizeType = typename LabelImageType::SizeType;
  using PointType = typename ImageType::PointType;
  using RegionType = typename ImageType::RegionType;
  using TransformType = itk::VersorRigid3DTransform<itk::SpacePrecisionType>;

  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Read the input image " << inputImage << std::endl;
  typename ImageType::Pointer input = itk::ReadImage<ImageType>(inputImage);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Read the input labels " << inputLabels << std::endl;
  typename LabelImageType::Pointer labels = itk::ReadImage<LabelImageType>(inputLabels);

  typename TransformType::Pointer directTransform = ReadTransform<TransformType>(transformFilename);
  directTransform->ApplyToImageMetadata(input);
  directTransform->ApplyToImageMetadata(labels);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Find the bounding box of the labels " << inputLabels << std::endl;
  auto bbSO = itk::ImageMaskSpatialObject<Dimension>::New();
  bbSO->SetImage(labels);
  bbSO->Update();

  // this is not the tightest, because BB is computed in index space internally
  const auto bb = bbSO->GetMyBoundingBoxInWorldSpace();
  PointType  start = bb->GetMinimum();
  PointType  end = bb->GetMaximum();

  SizeType size;
  for (unsigned d = 0; d < Dimension; d++)
  {
    size[d] = std::ceil((end[d] - start[d]) / input->GetSpacing()[d]);
  }

  typename LabelImageType::Pointer labelsAA = ResampleAxisAligned(labels, 0, start, size, true);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Find the tightest bounding box of the labels " << outputLabels << std::endl;
  bbSO->SetImage(labelsAA);
  RegionType bbRegion = bbSO->ComputeMyBoundingBoxInIndexSpace();

  using ROIType = itk::RegionOfInterestImageFilter<LabelImageType, LabelImageType>;
  typename ROIType::Pointer roiFilter = ROIType::New();
  roiFilter->SetInput(labelsAA);
  roiFilter->SetRegionOfInterest(bbRegion);
  roiFilter->Update();
  labelsAA = roiFilter->GetOutput();

  typename ImageType::Pointer outImage =
    ResampleAxisAligned(input, -1024, labelsAA->GetOrigin(), bbRegion.GetSize(), false);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Write the axis aligned image " << outputImage << std::endl;
  itk::WriteImage(outImage, outputImage, true);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Write the axis aligned labels " << outputLabels << std::endl;
  itk::WriteImage(labelsAA, outputLabels, true);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " All done!" << std::endl;
}

int
main(int argc, char * argv[])
{
  if (argc < 6)
  {
    std::cerr << "Usage:\n" << argv[0];
    std::cerr << " <InputImage> <InputLabels> <Transform> <OutputImage> <OutputLabels>" << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    mainProcessing<itk::Image<short, 3>>(argv[1], argv[2], argv[3], argv[4], argv[5]);
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
