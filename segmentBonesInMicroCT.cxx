#include <chrono>
#include <iostream>

#include "itkArray.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkMedianImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkConnectedComponentImageFilter.h"
#include "itkRelabelComponentImageFilter.h"
#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkNotImageFilter.h"
#include "itkSmoothingRecursiveGaussianImageFilter.h"
#include "itkMultiScaleHessianEnhancementImageFilter.h"
#include "itkDescoteauxEigenToScalarImageFilter.h"

auto startTime = std::chrono::steady_clock::now(); // global variable

template <typename TImage>
itk::SmartPointer<TImage>
ReadImage(std::string filename)
{
  using ReaderType = itk::ImageFileReader<TImage>;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(filename);
  reader->Update();
  itk::SmartPointer<TImage> out = reader->GetOutput();
  out->DisconnectPipeline();
  return out;
}

template <typename TImage>
void
WriteImage(TImage * out, std::string filename, bool compress)
{
  using WriterType = itk::ImageFileWriter<TImage>;
  typename WriterType::Pointer w = WriterType::New();
  w->SetInput(out);
  w->SetFileName(filename);
  w->SetUseCompression(compress);
  try
  {
    std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
    std::cout << diff.count() << " Updating " << filename << std::endl;
    out->Update();

    diff = std::chrono::steady_clock::now() - startTime;
    std::cout << diff.count() << " Writing " << filename << std::endl;
    w->Update();

    diff = std::chrono::steady_clock::now() - startTime;
    std::cout << diff.count() << " Done!" << std::endl;
  }
  catch (itk::ExceptionObject & error)
  {
    std::cerr << error << std::endl;
  }
}

template <typename TImage>
inline void
WriteImage(itk::SmartPointer<TImage> out, std::string filename, bool compress)
{
  WriteImage(out.GetPointer(), filename, compress);
}

// split the binary mask into components and remove the small islands
template <typename TImage>
itk::SmartPointer<TImage>
connectedComponentAnalysis(itk::SmartPointer<TImage> labelImage,
                           std::string               outFilename,
                           itk::IdentifierType &     numLabels)
{
  using ManyLabelImageType = itk::Image<itk::SizeValueType, TImage::ImageDimension>;
  using LabelerType = itk::ConnectedComponentImageFilter<TImage, ManyLabelImageType>;
  LabelerType::Pointer labeler = LabelerType::New();
  labeler->SetInput(labelImage);
  static unsigned invocationCount = 0;
  WriteImage(labeler->GetOutput(), outFilename + std::to_string(invocationCount) + "-cc-label.nrrd", true);

  using RelabelType = itk::RelabelComponentImageFilter<ManyLabelImageType, TImage>;
  typename RelabelType::Pointer relabeler = RelabelType::New();
  relabeler->SetInput(labeler->GetOutput());
  relabeler->SetMinimumObjectSize(1000);
  WriteImage(relabeler->GetOutput(), outFilename + std::to_string(invocationCount) + "-ccR-label.nrrd", true);
  ++invocationCount;

  relabeler->Update();
  numLabels = relabeler->GetNumberOfObjects();
  return relabeler->GetOutput();
}


// morphological dilation by thresholding the distance field
template <typename TImage>
itk::SmartPointer<TImage>
sdfDilate(itk::SmartPointer<TImage> labelImage, double radius, std::string outFilename)
{
  using RealPixelType = float;
  using RealImageType = itk::Image<RealPixelType, TImage::ImageDimension>;
  using DistanceFieldType = itk::SignedMaurerDistanceMapImageFilter<TImage, RealImageType>;

  typename DistanceFieldType::Pointer distF = DistanceFieldType::New();
  distF->SetInput(labelImage);
  distF->SetSquaredDistance(true);
  WriteImage(distF->GetOutput(), outFilename + "-dist-dilate.nrrd", false);

  using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, TImage>;
  typename FloatThresholdType::Pointer sdfTh = FloatThresholdType::New();
  sdfTh->SetInput(distF->GetOutput());
  sdfTh->SetUpperThreshold(radius * radius);
  WriteImage(sdfTh->GetOutput(), outFilename + "-dilate-label.nrrd", true);

  sdfTh->Update();
  return sdfTh->GetOutput();
}

// morphological erosion by thresholding the distance field
template <typename TImage>
itk::SmartPointer<TImage>
sdfErode(itk::SmartPointer<TImage> labelImage, double radius, std::string outFilename)
{
  // we need an inversion filter because Maurer's filter distances are not symmetrical
  // inside distances start at 0, while outside distances start at single spacing
  using NotType = itk::NotImageFilter<TImage, TImage>;
  NotType::Pointer negator = NotType::New();
  negator->SetInput(labelImage);
  WriteImage(negator->GetOutput(), outFilename + "-erode-Not-label.nrrd", true);

  using RealPixelType = float;
  using RealImageType = itk::Image<RealPixelType, TImage::ImageDimension>;
  using DistanceFieldType = itk::SignedMaurerDistanceMapImageFilter<TImage, RealImageType>;

  typename DistanceFieldType::Pointer distF = DistanceFieldType::New();
  distF->SetInput(negator->GetOutput());
  distF->SetSquaredDistance(true);
  WriteImage(distF->GetOutput(), outFilename + "-dist-erode.nrrd", false);

  using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, TImage>;
  typename FloatThresholdType::Pointer sdfTh = FloatThresholdType::New();
  sdfTh->SetInput(distF->GetOutput());
  sdfTh->SetLowerThreshold(radius * radius);
  WriteImage(sdfTh->GetOutput(), outFilename + "-erode-label.nrrd", true);

  sdfTh->Update();
  return sdfTh->GetOutput();
}

template <typename ImageType>
void
mainProcessing(typename ImageType::ConstPointer inImage, std::string outFilename, double corticalBoneThickness)
{
  itk::Array<double> sigmaArray(1);
  sigmaArray[0] = corticalBoneThickness;
  using LabelImageType = itk::Image<unsigned char, ImageType::ImageDimension>;
  using BinaryThresholdType = itk::BinaryThresholdImageFilter<ImageType, LabelImageType>;

  typename LabelImageType::Pointer gaussLabel;
  {
    using GaussType = itk::SmoothingRecursiveGaussianImageFilter<ImageType>;
    typename GaussType::Pointer gaussF = GaussType::New();
    gaussF->SetInput(inImage);
    gaussF->SetSigma(corticalBoneThickness);
    WriteImage(gaussF->GetOutput(), outFilename + "-gauss.nrrd", false);

    typename BinaryThresholdType::Pointer binTh2 = BinaryThresholdType::New();
    binTh2->SetInput(gaussF->GetOutput());
    binTh2->SetLowerThreshold(2000);
    WriteImage(binTh2->GetOutput(), outFilename + "-gauss-label.nrrd", true);
    gaussLabel = binTh2->GetOutput();
  }

  // TODO: improve MultiScaleHessianEnhancementImageFilter to allow streaming
  // because this filter uses a lot of memory
  typename LabelImageType::Pointer cortexLabel;
  {
    using RealImageType = itk::Image<float, ImageType::ImageDimension>;
    using MultiScaleHessianFilterType = itk::MultiScaleHessianEnhancementImageFilter<ImageType, RealImageType>;
    using DescoteauxEigenToScalarImageFilterType =
      itk::DescoteauxEigenToScalarImageFilter<MultiScaleHessianFilterType::EigenValueImageType, RealImageType>;
    MultiScaleHessianFilterType::Pointer multiScaleFilter = MultiScaleHessianFilterType::New();
    multiScaleFilter->SetInput(inImage);
    multiScaleFilter->SetSigmaArray(sigmaArray);
    DescoteauxEigenToScalarImageFilterType::Pointer descoFilter = DescoteauxEigenToScalarImageFilterType::New();
    multiScaleFilter->SetEigenToScalarImageFilter(descoFilter);

    WriteImage(multiScaleFilter->GetOutput(), outFilename + "-desco.nrrd", false);

    using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, LabelImageType>;
    typename FloatThresholdType::Pointer descoTh = FloatThresholdType::New();
    descoTh->SetInput(multiScaleFilter->GetOutput());
    descoTh->SetLowerThreshold(0.2);
    WriteImage(descoTh->GetOutput(), outFilename + "-desco-label.nrrd", true);
    cortexLabel = descoTh->GetOutput();
  }


  typename BinaryThresholdType::Pointer binTh = BinaryThresholdType::New();
  binTh->SetInput(inImage);
  binTh->SetLowerThreshold(1500); // TODO: start from an even higher threshold, so bones are well separated
  WriteImage(binTh->GetOutput(), outFilename + "-bin1-label.nrrd", true);
  typename LabelImageType::Pointer thLabel = binTh->GetOutput();

  itk::MultiThreaderBase::Pointer mt = itk::MultiThreaderBase::New();
  // we will update cortexLabel with information from gaussLabel and thLabel
  using RegionType = typename LabelImageType::RegionType;
  RegionType wholeImage = inImage->GetLargestPossibleRegion();
  mt->ParallelizeImageRegion<ImageType::ImageDimension>(
    wholeImage,
    [cortexLabel, gaussLabel, thLabel](RegionType region) {
      itk::ImageRegionConstIterator<LabelImageType> gIt(gaussLabel, region);
      itk::ImageRegionConstIterator<LabelImageType> tIt(thLabel, region);
      itk::ImageRegionIterator<LabelImageType>      cIt(cortexLabel, region);
      for (; !cIt.IsAtEnd(); ++gIt, ++tIt, ++cIt)
      {
        unsigned char p = cIt.Get() || gIt.Get();
        p = p && tIt.Get();
        cIt.Set(p);
      }
    },
    nullptr);
  WriteImage(cortexLabel, outFilename + "-cortex-label.nrrd", true);
  // typename LabelImageType::Pointer cortexLabel = ReadImage<LabelImageType>(outFilename + "-cortex-label.nrrd");
  typename LabelImageType::Pointer cortexEroded =
    sdfErode(cortexLabel, 0.5 * corticalBoneThickness, outFilename + "-cortex-eroded-label.nrrd");

  typename LabelImageType::Pointer finalBones = LabelImageType::New();
  finalBones->CopyInformation(inImage);
  finalBones->SetRegions(wholeImage);
  finalBones->Allocate(true);

  // do morphological processing per bone, to avoid merging bones which are close to each other
  itk::IdentifierType numBones = 0;

  typename LabelImageType::Pointer bones = connectedComponentAnalysis(thLabel, outFilename, numBones);
  // we might not even get to this point if there are more bones than 255
  // we need 3 labels per bone, one each for cortical, trabecular and marrow
  itkAssertOrThrowMacro(numBones <= 85, "There are too many bones to fit into uchar");
  for (unsigned bone = 1; bone <= numBones; bone++)
  {
    typename LabelImageType::Pointer thBone = LabelImageType::New();
    thBone->CopyInformation(inImage);
    thBone->SetRegions(wholeImage);
    thBone->Allocate();

    // TODO: also calculate expanded bounding box
    // so the subsequent operations don't need to process the whole image
    mt->ParallelizeImageRegion<ImageType::ImageDimension>(
      wholeImage,
      [thBone, bones, cortexEroded, bone](RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> bIt(bones, region);
        itk::ImageRegionConstIterator<LabelImageType> cIt(cortexEroded, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(thBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++cIt, ++oIt)
        {
          bool p = (bIt.Get() == bone) && (cIt.Get() == 0);
          oIt.Set(p);
        }
      },
      nullptr);
    std::string boneFilename = outFilename + "-bone" + std::to_string(bone);
    WriteImage(thBone, boneFilename + "-trabecular-label.nrrd", true);

    typename LabelImageType::Pointer dilatedBone = sdfDilate(thBone, 3.0 * corticalBoneThickness, boneFilename);
    typename LabelImageType::Pointer erodedBone = sdfErode(dilatedBone, 4.0 * corticalBoneThickness, boneFilename);
    dilatedBone = sdfDilate(erodedBone, 1.0 * corticalBoneThickness, boneFilename);

    // now do the same for marrow, seeding from cortical and trabecular bone
    mt->ParallelizeImageRegion<ImageType::ImageDimension>(
      wholeImage,
      [thBone, erodedBone](RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> bIt(erodedBone, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(thBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++oIt)
        {
          oIt.Set(bIt.Get() || oIt.Get());
        }
      },
      nullptr);
    typename LabelImageType::Pointer dilatedMarrow = sdfDilate(thBone, 5.0 * corticalBoneThickness, boneFilename);
    typename LabelImageType::Pointer erodedMarrow = sdfErode(dilatedMarrow, 6.0 * corticalBoneThickness, boneFilename);

    // now combine them
    mt->ParallelizeImageRegion<ImageType::ImageDimension>(
      wholeImage,
      [finalBones, erodedMarrow, dilatedBone, cortexLabel, bones, bone](RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> mIt(erodedMarrow, region);
        itk::ImageRegionConstIterator<LabelImageType> bIt(dilatedBone, region);
        itk::ImageRegionConstIterator<LabelImageType> cIt(cortexLabel, region);
        itk::ImageRegionConstIterator<LabelImageType> nIt(bones, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(finalBones, region);
        for (; !oIt.IsAtEnd(); ++mIt, ++bIt, ++cIt, ++nIt, ++oIt)
        {
          if (cIt.Get() && nIt.Get() == bone)
          {
            oIt.Set(3 * bone - 2);
          }
          else if (bIt.Get())
          {
            oIt.Set(3 * bone - 1);
          }
          else if (mIt.Get())
          {
            oIt.Set(3 * bone);
          }
          // else this is background
        }
      },
      nullptr);
    WriteImage(finalBones, boneFilename + "-label.nrrd", true);
  }
  WriteImage(finalBones, outFilename, true);
}

int
main(int argc, char * argv[])
{
  if (argc < 3)
  {
    std::cerr << "Usage: " << std::endl;
    std::cerr << argv[0];
    std::cerr << " <InputFileName> <OutputSegmentation> [corticalBoneThickness]";
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    std::string inputFileName = argv[1];
    std::string outputFileName = argv[2];
    double      corticalBoneThickness = 0.1;
    if (argc > 3)
    {
      corticalBoneThickness = std::stod(argv[3]);
    }

    std::cout.precision(4);
    std::cout << " InputFilePath: " << inputFileName << std::endl;
    std::cout << "OutputFilePath: " << outputFileName << std::endl;
    std::cout << "Cortical Bone Thickness: " << corticalBoneThickness << std::endl;
    std::cout << std::endl;

    constexpr unsigned ImageDimension = 3;
    using InputPixelType = short;
    using InputImageType = itk::Image<InputPixelType, ImageDimension>;

    typename InputImageType::Pointer image = ReadImage<InputImageType>(inputFileName);

    using MedianType = itk::MedianImageFilter<InputImageType, InputImageType>;
    MedianType::Pointer median = MedianType::New();
    median->SetInput(image);
    WriteImage(median->GetOutput(), outputFileName + "-median.nrrd", false);
    image = median->GetOutput();
    image->DisconnectPipeline();

    mainProcessing<InputImageType>(image, outputFileName, corticalBoneThickness);
    return EXIT_SUCCESS;
  }
  catch (itk::ExceptionObject & exc)
  {
    std::cerr << exc;
  }
  catch (std::runtime_error & exc)
  {
    std::cerr << exc.what();
  }
  catch (...)
  {
    std::cerr << "Unknown error has occurred" << std::endl;
  }
  return EXIT_FAILURE;
}
