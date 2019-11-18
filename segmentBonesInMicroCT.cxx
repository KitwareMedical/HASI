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
#include "itkNeighborhoodConnectedImageFilter.h"
//#include "itkRegionOfInterestImageFilter.h"


auto     startTime = std::chrono::steady_clock::now();
unsigned runDebugLevel = 0;

template <typename TImage>
itk::SmartPointer<TImage>
ReadImage(std::string filename)
{
  using ReaderType = itk::ImageFileReader<TImage>;
  typename ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(filename);
  reader->Update();
  itk::SmartPointer<TImage> out = reader->GetOutput();
  out->DisconnectPipeline();
  return out;
}

template <typename TImage>
void
UpdateAndWrite(TImage * out, std::string filename, bool compress, unsigned debugLevel)
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

    if (runDebugLevel >= debugLevel) // we should write this image
    {
      diff = std::chrono::steady_clock::now() - startTime;
      std::cout << diff.count() << " Writing " << filename << std::endl;
      w->Update();
    }

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
UpdateAndWrite(itk::SmartPointer<TImage> out, std::string filename, bool compress, unsigned debugLevel)
{
  UpdateAndWrite(out.GetPointer(), filename, compress, debugLevel);
}

// split the binary mask into components and remove the small islands
template <typename TImage>
itk::SmartPointer<TImage>
connectedComponentAnalysis(itk::SmartPointer<TImage> labelImage,
                           std::string               outFilename,
                           itk::IdentifierType &     numLabels,
                           unsigned                  debugLevel)
{
  using ManyLabelImageType = itk::Image<itk::SizeValueType, TImage::ImageDimension>;
  using LabelerType = itk::ConnectedComponentImageFilter<TImage, ManyLabelImageType>;
  typename LabelerType::Pointer labeler = LabelerType::New();
  labeler->SetInput(labelImage);
  static unsigned invocationCount = 0;
  UpdateAndWrite(
    labeler->GetOutput(), outFilename + std::to_string(invocationCount) + "-cc-label.nrrd", true, debugLevel + 1);

  using RelabelType = itk::RelabelComponentImageFilter<ManyLabelImageType, TImage>;
  typename RelabelType::Pointer relabeler = RelabelType::New();
  relabeler->SetInput(labeler->GetOutput());
  relabeler->SetMinimumObjectSize(1000);
  UpdateAndWrite(
    relabeler->GetOutput(), outFilename + std::to_string(invocationCount) + "-ccR-label.nrrd", true, debugLevel);
  ++invocationCount;

  numLabels = relabeler->GetNumberOfObjects();
  return relabeler->GetOutput();
}

// signed distance field
template <unsigned Dimension>
itk::SmartPointer<itk::Image<float, Dimension>>
sdf(itk::SmartPointer<itk::Image<unsigned char, Dimension>> labelImage, std::string outFilename, unsigned debugLevel)
{
  using RealImageType = itk::Image<float, Dimension>;
  using LabelImageType = itk::Image<unsigned char, Dimension>;
  using DistanceFieldType = itk::SignedMaurerDistanceMapImageFilter<LabelImageType, RealImageType>;

  typename DistanceFieldType::Pointer distF = DistanceFieldType::New();
  distF->SetInput(labelImage);
  distF->SetSquaredDistance(true);
  UpdateAndWrite(distF->GetOutput(), outFilename, false, debugLevel);
  typename RealImageType::Pointer dist = distF->GetOutput();
  dist->DisconnectPipeline();
  return dist;
}

// morphological dilation by thresholding the distance field
template <typename TImage>
itk::SmartPointer<TImage>
sdfDilate(itk::SmartPointer<TImage> labelImage, double radius, std::string outFilename, unsigned debugLevel)
{
  using RealImageType = itk::Image<float, TImage::ImageDimension>;
  using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, TImage>;
  typename FloatThresholdType::Pointer sdfTh = FloatThresholdType::New();
  sdfTh->SetInput(sdf(labelImage, outFilename + "-dilate-dist.nrrd", debugLevel + 1));
  sdfTh->SetUpperThreshold(radius * radius);
  UpdateAndWrite(sdfTh->GetOutput(), outFilename + "-dilate-label.nrrd", true, debugLevel);

  return sdfTh->GetOutput();
}

// morphological erosion by thresholding the distance field
template <typename TImage>
itk::SmartPointer<TImage>
sdfErode(itk::SmartPointer<TImage> labelImage, double radius, std::string outFilename, unsigned debugLevel)
{
  // we need an inversion filter because Maurer's filter distances are not symmetrical
  // inside distances start at 0, while outside distances start at single spacing
  using NotType = itk::NotImageFilter<TImage, TImage>;
  typename NotType::Pointer negator = NotType::New();
  negator->SetInput(labelImage);
  UpdateAndWrite(negator->GetOutput(), outFilename + "-erode-Not-label.nrrd", true, debugLevel + 2);

  using RealImageType = itk::Image<float, TImage::ImageDimension>;
  using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, TImage>;
  typename FloatThresholdType::Pointer sdfTh = FloatThresholdType::New();
  sdfTh->SetInput(sdf<TImage::ImageDimension>(negator->GetOutput(), outFilename + "-erode-dist.nrrd", debugLevel + 1));
  sdfTh->SetLowerThreshold(radius * radius);
  UpdateAndWrite(sdfTh->GetOutput(), outFilename + "-erode-label.nrrd", true, debugLevel);

  return sdfTh->GetOutput();
}

template <typename ImageType>
void
mainProcessing(typename ImageType::ConstPointer inImage, std::string outFilename, double corticalBoneThickness)
{
  itk::Array<double> sigmaArray(1);
  sigmaArray[0] = corticalBoneThickness;
  constexpr unsigned Dimension = ImageType::ImageDimension;
  using RealImageType = itk::Image<float, Dimension>;
  using LabelImageType = itk::Image<unsigned char, Dimension>;
  using BinaryThresholdType = itk::BinaryThresholdImageFilter<ImageType, LabelImageType>;

  typename LabelImageType::Pointer gaussLabel;
  {
    using GaussType = itk::SmoothingRecursiveGaussianImageFilter<ImageType>;
    typename GaussType::Pointer gaussF = GaussType::New();
    gaussF->SetInput(inImage);
    gaussF->SetSigma(corticalBoneThickness);
    UpdateAndWrite(gaussF->GetOutput(), outFilename + "-gauss.nrrd", false, 3);

    typename BinaryThresholdType::Pointer binTh2 = BinaryThresholdType::New();
    binTh2->SetInput(gaussF->GetOutput());
    binTh2->SetLowerThreshold(2000);
    UpdateAndWrite(binTh2->GetOutput(), outFilename + "-gauss-label.nrrd", true, 2);
    gaussLabel = binTh2->GetOutput();
  }

  // TODO: improve MultiScaleHessianEnhancementImageFilter to allow streaming
  // because this filter uses a lot of memory
  typename LabelImageType::Pointer cortexLabel;
  {
    using RealImageType = itk::Image<float, Dimension>;
    using MultiScaleHessianFilterType = itk::MultiScaleHessianEnhancementImageFilter<ImageType, RealImageType>;
    using DescoteauxEigenToScalarImageFilterType =
      itk::DescoteauxEigenToScalarImageFilter<typename MultiScaleHessianFilterType::EigenValueImageType, RealImageType>;
    typename MultiScaleHessianFilterType::Pointer multiScaleFilter = MultiScaleHessianFilterType::New();
    multiScaleFilter->SetInput(inImage);
    multiScaleFilter->SetSigmaArray(sigmaArray);
    typename DescoteauxEigenToScalarImageFilterType::Pointer descoFilter =
      DescoteauxEigenToScalarImageFilterType::New();
    multiScaleFilter->SetEigenToScalarImageFilter(descoFilter);

    UpdateAndWrite(multiScaleFilter->GetOutput(), outFilename + "-desco.nrrd", false, 1);

    using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, LabelImageType>;
    typename FloatThresholdType::Pointer descoTh = FloatThresholdType::New();
    descoTh->SetInput(multiScaleFilter->GetOutput());
    descoTh->SetLowerThreshold(0.1);
    UpdateAndWrite(descoTh->GetOutput(), outFilename + "-desco-label.nrrd", true, 1);
    cortexLabel = descoTh->GetOutput();
  }


  typename BinaryThresholdType::Pointer binTh = BinaryThresholdType::New();
  binTh->SetInput(inImage);
  binTh->SetLowerThreshold(2500); // start from a high threshold, so bones are well separated
  UpdateAndWrite(binTh->GetOutput(), outFilename + "-bin1-label.nrrd", true, 2);
  typename LabelImageType::Pointer thLabel = binTh->GetOutput();

  itk::MultiThreaderBase::Pointer mt = itk::MultiThreaderBase::New();
  // we will update cortexLabel with information from gaussLabel and thLabel
  using RegionType = typename LabelImageType::RegionType;
  RegionType wholeImage = inImage->GetLargestPossibleRegion();
  mt->ParallelizeImageRegion<Dimension>(
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
  UpdateAndWrite(cortexLabel, outFilename + "-cortex-label.nrrd", true, 1);
  typename LabelImageType::Pointer cortexEroded =
    sdfErode(cortexLabel, 0.5 * corticalBoneThickness, outFilename + "-cortex-eroded", 2);

  typename LabelImageType::Pointer finalBones = LabelImageType::New();
  finalBones->CopyInformation(inImage);
  finalBones->SetRegions(wholeImage);
  finalBones->Allocate(true);

  // do morphological processing per bone, to avoid merging bones which are close to each other
  itk::IdentifierType numBones = 0;

  typename LabelImageType::Pointer bones = connectedComponentAnalysis(thLabel, outFilename, numBones, 1);
  // we might not even get to this point if there are more than 255 bones
  // we need 3 labels per bone, one each for cortical, trabecular and marrow
  itkAssertOrThrowMacro(numBones <= 85, "There are too many bones to fit into uchar");

  typename RealImageType::Pointer boneDist = sdf(bones, outFilename + "-bones-dist.nrrd", 1);

  using IndexType = typename LabelImageType::IndexType;
  std::vector<IndexType> minIndices(numBones + 1, IndexType::Filled(itk::NumericTraits<itk::IndexValueType>::max()));
  std::vector<IndexType> maxIndices(numBones + 1,
                                    IndexType::Filled(itk::NumericTraits<itk::IndexValueType>::NonpositiveMin()));

  typename LabelImageType::Pointer trabecularBones = LabelImageType::New();
  trabecularBones->CopyInformation(inImage);
  trabecularBones->SetRegions(wholeImage);
  trabecularBones->Allocate(true);
  {
    itk::ImageRegionConstIterator<LabelImageType>     bIt(bones, wholeImage);
    itk::ImageRegionConstIterator<LabelImageType>     cIt(cortexEroded, wholeImage);
    itk::ImageRegionIteratorWithIndex<LabelImageType> tIt(trabecularBones, wholeImage);
    for (; !tIt.IsAtEnd(); ++tIt, ++bIt, ++cIt)
    {
      unsigned char bone = bIt.Get();
      if (bone > 0)
      {
        if (cIt.Get() == 0)
        {
          tIt.Set(bone);
        }

        // for determining bone's bounding box
        IndexType ind = tIt.GetIndex();
        for (unsigned d = 0; d < Dimension; d++)
        {
          if (ind[d] < minIndices[bone][d])
          {
            minIndices[bone][d] = ind[d];
          }
          if (ind[d] > maxIndices[bone][d])
          {
            maxIndices[bone][d] = ind[d];
          }
        }
      }
    }
  }
  UpdateAndWrite(trabecularBones, outFilename + "-trabecular-label.nrrd", true, 1);

  const double maxRadius = 8.0 * corticalBoneThickness; // allow some room for imperfect intermediate steps
  double       avgSpacing = 1.0;
  IndexType    opSize; // maximum extent of morphological operations
  for (unsigned d = 0; d < Dimension; d++)
  {
    opSize[d] = std::ceil(maxRadius / inImage->GetSpacing()[d]);
    avgSpacing *= inImage->GetSpacing()[d];
  }
  avgSpacing = std::pow(avgSpacing, 1.0 / Dimension); // geometric average preserves voxel volume

  float epsDist = 0.001 * avgSpacing; // epsilon for distance comparisons


  // per-bone processing
  for (unsigned bone = 1; bone <= numBones; bone++)
  {
    std::string boneFilename = outFilename + "-bone" + std::to_string(bone);

    // calculate expanded bounding box, so the subsequent operations don't need to process the whole image
    RegionType boneRegion;         // tight bounding box
    RegionType expandedBoneRegion; // bounding box with room for morphological operations
    for (unsigned d = 0; d < Dimension; d++)
    {
      boneRegion.SetIndex(d, minIndices[bone][d]);
      boneRegion.SetSize(d, maxIndices[bone][d] - minIndices[bone][d] + 1);

      expandedBoneRegion.SetIndex(d, minIndices[bone][d] - opSize[d]);
      expandedBoneRegion.SetSize(d, maxIndices[bone][d] - minIndices[bone][d] + 1 + 2 * opSize[d]);
    }
    RegionType safeBoneRegion = expandedBoneRegion;
    safeBoneRegion.Crop(wholeImage); // restrict to image size

    typename LabelImageType::Pointer thisBone = LabelImageType::New();
    thisBone->CopyInformation(inImage);
    thisBone->SetRegions(expandedBoneRegion);
    thisBone->Allocate(true);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [thisBone, bones, bone](RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> bIt(bones, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(thisBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++oIt)
        {
          if (bIt.Get() == bone)
          {
            oIt.Set(bone);
          }
        }
      },
      nullptr);
    typename RealImageType::Pointer thisDist = sdf(thisBone, boneFilename + "-dist.nrrd", 2);

    constexpr typename ImageType::PixelType background = -4096;

    typename ImageType::Pointer partialInput = ImageType::New();
    partialInput->CopyInformation(inImage);
    partialInput->SetRegions(safeBoneRegion);
    partialInput->Allocate(false);
    partialInput->FillBuffer(background);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [partialInput, inImage, thisDist, boneDist, background, epsDist](RegionType region) {
        itk::ImageRegionConstIterator<RealImageType> tIt(thisDist, region);
        itk::ImageRegionConstIterator<RealImageType> gIt(boneDist, region);
        itk::ImageRegionConstIterator<ImageType>     iIt(inImage, region);
        itk::ImageRegionIterator<ImageType>          oIt(partialInput, region);
        for (; !oIt.IsAtEnd(); ++iIt, ++tIt, ++gIt, ++oIt)
        {
          if (std::abs(tIt.Get() - gIt.Get()) < epsDist)
          {
            oIt.Set(iIt.Get());
          }
        }
      },
      nullptr);
    UpdateAndWrite(partialInput, boneFilename + "-partial-input.nrrd", true, 2);

    using ConnectedFilterType = itk::NeighborhoodConnectedImageFilter<ImageType, LabelImageType>;
    typename ConnectedFilterType::Pointer neighborhoodConnected = ConnectedFilterType::New();
    neighborhoodConnected->SetInput(partialInput);
    neighborhoodConnected->SetLower(1500); // use a lower threshold here, so we capture more of trabecular bone
    itk::ImageRegionConstIteratorWithIndex<LabelImageType> tIt(bones, boneRegion);
    for (; !tIt.IsAtEnd(); ++tIt)
    {
      if (tIt.Get() == bone)
      {
        neighborhoodConnected->AddSeed(tIt.GetIndex());
      }
    }
    UpdateAndWrite(neighborhoodConnected->GetOutput(), boneFilename + "-trabecular-label.nrrd", true, 3);
    typename LabelImageType::Pointer thBone = neighborhoodConnected->GetOutput();

    typename LabelImageType::Pointer dilatedBone =
      sdfDilate(thBone, 3.0 * corticalBoneThickness, boneFilename + "-trabecular1", 3);
    typename LabelImageType::Pointer erodedBone =
      sdfErode(dilatedBone, 4.0 * corticalBoneThickness, boneFilename + "-trabecular2", 3);
    dilatedBone = sdfDilate(erodedBone, 1.0 * corticalBoneThickness, boneFilename + "-trabecular3", 3);

    // now do the same for marrow, seeding from cortical and trabecular bone
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [thBone, erodedBone](RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> bIt(erodedBone, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(thBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++oIt)
        {
          oIt.Set(bIt.Get() || oIt.Get());
        }
      },
      nullptr);
    typename LabelImageType::Pointer dilatedMarrow =
      sdfDilate(thBone, 5.0 * corticalBoneThickness, boneFilename + "-marrow", 3);
    typename LabelImageType::Pointer erodedMarrow =
      sdfErode(dilatedMarrow, 6.0 * corticalBoneThickness, boneFilename + "-marrow", 3);

    // now combine them, clipping them to the bone mask implied by partialInput
    mt->ParallelizeImageRegion<Dimension>(
      safeBoneRegion,
      [finalBones, erodedMarrow, dilatedBone, cortexLabel, partialInput, bone, background](RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> mIt(erodedMarrow, region);
        itk::ImageRegionConstIterator<LabelImageType> bIt(dilatedBone, region);
        itk::ImageRegionConstIterator<LabelImageType> cIt(cortexLabel, region);
        itk::ImageRegionConstIterator<ImageType>      iIt(partialInput, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(finalBones, region);
        for (; !oIt.IsAtEnd(); ++mIt, ++bIt, ++cIt, ++iIt, ++oIt)
        {
          if (iIt.Get() > background)
          {
            if (cIt.Get())
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
          }
          // else this is background
        }
      },
      nullptr);
    UpdateAndWrite(finalBones, boneFilename + "-label.nrrd", true, 2);
  }
  UpdateAndWrite(finalBones, outFilename + "-label.nrrd", true, 0); // always write
}

int
main(int argc, char * argv[])
{
  if (argc < 3)
  {
    std::cerr << "Usage: " << std::endl;
    std::cerr << argv[0];
    std::cerr << " <InputFileName> <OutputFileName> [corticalBoneThickness] [debugLevel]";
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
    if (argc > 4)
    {
      runDebugLevel = std::stoul(argv[4]);
    }

    std::cout.precision(4);
    std::cout << " InputFilePath: " << inputFileName << std::endl;
    std::cout << "OutputFileBase: " << outputFileName << std::endl;
    std::cout << "Cortical Bone Thickness: " << corticalBoneThickness << std::endl;
    std::cout << std::endl;

    constexpr unsigned ImageDimension = 3;
    using InputPixelType = short;
    using InputImageType = itk::Image<InputPixelType, ImageDimension>;

    typename InputImageType::Pointer image = ReadImage<InputImageType>(inputFileName);

    using MedianType = itk::MedianImageFilter<InputImageType, InputImageType>;
    MedianType::Pointer median = MedianType::New();
    median->SetInput(image);
    UpdateAndWrite(median->GetOutput(), outputFileName + "-median.nrrd", false, 1);
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
