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
#include "itkDescoteauxEigenToMeasureImageFilter.h"
#include "itkDescoteauxEigenToMeasureParameterEstimationFilter.h"
#include "itkNeighborhoodConnectedImageFilter.h"
#include "itkConstantPadImageFilter.h"
#include "itkBinaryFillholeImageFilter.h"


auto     startTime = std::chrono::steady_clock::now();
unsigned runDebugLevel = 0;

template <typename TImage>
void
UpdateAndWrite(TImage * out, std::string filename, bool compress, unsigned debugLevel)
{
  try
  {
    std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
    std::cout << diff.count() << " Updating " << filename << std::endl;
    out->Update();

    if (runDebugLevel >= debugLevel) // we should write this image
    {
      diff = std::chrono::steady_clock::now() - startTime;
      std::cout << diff.count() << " Writing " << filename << std::endl;
      itk::WriteImage(out, filename, compress);
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

// zero-pad an image
template <typename TImage>
itk::SmartPointer<TImage>
zeroPad(itk::SmartPointer<TImage>         image,
        itk::Size<TImage::ImageDimension> padSize,
        std::string                       outFilename,
        unsigned                          debugLevel)
{
  using PadType = itk::ConstantPadImageFilter<TImage, TImage>;
  typename PadType::Pointer padder = PadType::New();
  padder->SetInput(image);
  padder->SetPadBound(padSize);
  padder->Update();
  UpdateAndWrite(padder->GetOutput(), outFilename, true, debugLevel);
  return padder->GetOutput();
}

template <typename ImageType>
void
mainProcessing(typename ImageType::ConstPointer inImage,
               std::string                      outFilename,
               double                           corticalBoneThickness,
               itk::IdentifierType              boneCount)
{
  itk::Array<double> sigmaArray(1);
  sigmaArray[0] = corticalBoneThickness;
  constexpr unsigned Dimension = ImageType::ImageDimension;
  using RealImageType = itk::Image<float, Dimension>;
  using LabelImageType = itk::Image<unsigned char, Dimension>;
  using BinaryThresholdType = itk::BinaryThresholdImageFilter<ImageType, LabelImageType>;
  using RegionType = typename LabelImageType::RegionType;
  using IndexType = typename LabelImageType::IndexType;
  using SizeType = typename LabelImageType::SizeType;

  const double maxRadius = 8.0 * corticalBoneThickness; // allow some room for imperfect intermediate steps
  double       avgSpacing = 1.0;
  SizeType     opSize; // maximum extent of morphological operations
  for (unsigned d = 0; d < Dimension; d++)
  {
    opSize[d] = std::ceil(maxRadius / inImage->GetSpacing()[d]);
    avgSpacing *= inImage->GetSpacing()[d];
  }
  avgSpacing = std::pow(avgSpacing, 1.0 / Dimension); // geometric average preserves voxel volume
  float epsDist = 0.001 * avgSpacing;                 // epsilon for distance comparisons

  RegionType wholeImage = inImage->GetLargestPossibleRegion();

  // extra padding so morphological operations don't introduce boundary effects
  RegionType paddedWholeImage = wholeImage;
  paddedWholeImage.PadByRadius(opSize);

  // we will do pixel-wise operation in a multi-threaded manner
  itk::MultiThreaderBase::Pointer mt = itk::MultiThreaderBase::New();

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

  typename LabelImageType::Pointer descoLabel;
  {
    using MultiScaleHessianFilterType = itk::MultiScaleHessianEnhancementImageFilter<ImageType, RealImageType>;
    using EigenValueImageType = typename MultiScaleHessianFilterType::EigenValueImageType;
    using DescoteauxEigenToScalarImageFilterType =
      itk::DescoteauxEigenToMeasureImageFilter<EigenValueImageType, RealImageType>;
    using DescoteauxMeasureEstimationType = itk::DescoteauxEigenToMeasureParameterEstimationFilter<EigenValueImageType>;

    typename MultiScaleHessianFilterType::Pointer multiScaleFilter = MultiScaleHessianFilterType::New();
    multiScaleFilter->SetInput(inImage);
    multiScaleFilter->SetSigmaArray(sigmaArray);
    typename DescoteauxEigenToScalarImageFilterType::Pointer descoFilter =
      DescoteauxEigenToScalarImageFilterType::New();
    multiScaleFilter->SetEigenToMeasureImageFilter(descoFilter);
    typename DescoteauxMeasureEstimationType::Pointer descoEstimator = DescoteauxMeasureEstimationType::New();
    multiScaleFilter->SetEigenToMeasureParameterEstimationFilter(descoEstimator);

    UpdateAndWrite(multiScaleFilter->GetOutput(), outFilename + "-desco.nrrd", false, 2);

    using FloatThresholdType = itk::BinaryThresholdImageFilter<RealImageType, LabelImageType>;
    typename FloatThresholdType::Pointer descoTh = FloatThresholdType::New();
    descoTh->SetInput(multiScaleFilter->GetOutput());
    descoTh->SetLowerThreshold(0.1);
    UpdateAndWrite(descoTh->GetOutput(), outFilename + "-desco-label.nrrd", true, 2);
    descoLabel = descoTh->GetOutput();
  }


  typename BinaryThresholdType::Pointer binTh = BinaryThresholdType::New();
  binTh->SetInput(inImage);
  binTh->SetLowerThreshold(5000); // start from a high threshold, so bones are well separated
  UpdateAndWrite(binTh->GetOutput(), outFilename + "-bin1-label.nrrd", true, 2);
  typename LabelImageType::Pointer thLabel = binTh->GetOutput();

  // create cortexLabel with information from descoLabel, gaussLabel and thLabel
  typename LabelImageType::Pointer cortexLabel = LabelImageType::New();
  cortexLabel->CopyInformation(inImage);
  cortexLabel->SetRegions(paddedWholeImage);
  cortexLabel->Allocate(true);
  mt->ParallelizeImageRegion<Dimension>(
    wholeImage,
    [descoLabel, gaussLabel, thLabel, cortexLabel](const RegionType region) {
      itk::ImageRegionConstIterator<LabelImageType> gIt(gaussLabel, region);
      itk::ImageRegionConstIterator<LabelImageType> tIt(thLabel, region);
      itk::ImageRegionConstIterator<LabelImageType> dIt(descoLabel, region);
      itk::ImageRegionIterator<LabelImageType>      cIt(cortexLabel, region);
      for (; !cIt.IsAtEnd(); ++gIt, ++tIt, ++dIt, ++cIt)
      {
        unsigned char p = dIt.Get() || gIt.Get();
        p = p && tIt.Get();
        if (p)
        {
          cIt.Set(p);
        }
      }
    },
    nullptr);
  UpdateAndWrite(cortexLabel, outFilename + "-cortex-label.nrrd", true, 2);
  typename LabelImageType::Pointer cortexEroded =
    sdfErode(cortexLabel, 0.5 * corticalBoneThickness, outFilename + "-cortex-eroded", 2);
  descoLabel = nullptr; // deallocate it
  gaussLabel = nullptr; // deallocate it

  typename LabelImageType::Pointer finalBones = LabelImageType::New();
  finalBones->CopyInformation(inImage);
  finalBones->SetRegions(wholeImage);
  finalBones->Allocate(true);

  typename LabelImageType::Pointer splitBones = LabelImageType::New();
  splitBones->CopyInformation(inImage);
  splitBones->SetRegions(wholeImage);
  splitBones->Allocate(true);

  // do morphological processing per bone, to avoid merging bones which are close to each other
  itk::IdentifierType numBones = 0;

  typename LabelImageType::Pointer bones = connectedComponentAnalysis(thLabel, outFilename, numBones, 3);
  // we might not even get to this point if there are more than 255 bones
  // we need 3 labels per bone, one each for cortical, trabecular and marrow
  itkAssertOrThrowMacro(numBones <= 85, "There are too many bones to fit into uchar");
  UpdateAndWrite(bones, outFilename + "-bones-label.nrrd", true, 1);

  bones = zeroPad(bones, opSize, outFilename + "-bonesPad-label.nrrd", 3);
  typename RealImageType::Pointer boneDist = sdf(bones, outFilename + "-bones-dist.nrrd", 3);

  // calculate bounding box for each bone
  std::vector<IndexType> minIndices(numBones + 1, IndexType::Filled(itk::NumericTraits<itk::IndexValueType>::max()));
  std::vector<IndexType> maxIndices(numBones + 1,
                                    IndexType::Filled(itk::NumericTraits<itk::IndexValueType>::NonpositiveMin()));
  std::vector<unsigned char> replacedBy(numBones + 1, 0);
  {
    itk::ImageRegionConstIteratorWithIndex<LabelImageType> bIt(bones, wholeImage);
    itk::ImageRegionConstIterator<LabelImageType>          cIt(cortexEroded, wholeImage);
    for (; !bIt.IsAtEnd(); ++bIt, ++cIt)
    {
      unsigned char bone = bIt.Get();
      if (bone > 0)
      {
        // for determining bone's bounding box
        IndexType ind = bIt.GetIndex();
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


  // per-bone processing
  for (unsigned bone = 1; bone <= std::min(numBones, boneCount); bone++)
  {
    if (replacedBy[bone] > 0)
    {
      std::cout << "Bone " << bone << " was an island inside bone " << unsigned(replacedBy[bone]) << std::endl;
      continue; // next bone
    }

    std::string boneFilename = outFilename + "-bone" + std::to_string(bone);

    // calculate expanded bounding box, so the subsequent operations don't need to process the whole image
    RegionType boneRegion;         // tight bounding box
    RegionType expandedBoneRegion; // bounding box with room for morphological operations
    for (unsigned d = 0; d < Dimension; d++)
    {
      boneRegion.SetIndex(d, minIndices[bone][d]);
      boneRegion.SetSize(d, maxIndices[bone][d] - minIndices[bone][d] + 1);

      itk::IndexValueType opSizeD = opSize[d]; // a signed value
      expandedBoneRegion.SetIndex(d, minIndices[bone][d] - opSizeD);
      expandedBoneRegion.SetSize(d, maxIndices[bone][d] - minIndices[bone][d] + 1 + 2 * opSizeD);
    }
    RegionType safeBoneRegion = expandedBoneRegion;
    safeBoneRegion.Crop(wholeImage); // restrict to image size

    typename LabelImageType::Pointer thisBone = LabelImageType::New();
    thisBone->CopyInformation(inImage);
    thisBone->SetRegions(expandedBoneRegion);
    thisBone->Allocate(true);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [thisBone, bones, bone](const RegionType region) {
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
    thisBone = nullptr; // deallocate it

    typename LabelImageType::Pointer boneBasin = LabelImageType::New();
    boneBasin->CopyInformation(inImage);
    boneBasin->SetRegions(safeBoneRegion);
    boneBasin->Allocate(true);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [boneBasin, thisDist, boneDist, epsDist](const RegionType region) {
        itk::ImageRegionConstIterator<RealImageType> tIt(thisDist, region);
        itk::ImageRegionConstIterator<RealImageType> gIt(boneDist, region);
        itk::ImageRegionIterator<LabelImageType>     oIt(boneBasin, region);
        for (; !oIt.IsAtEnd(); ++tIt, ++gIt, ++oIt)
        {
          if (std::abs(tIt.Get() - gIt.Get()) < epsDist)
          {
            oIt.Set(1);
          }
        }
      },
      nullptr);
    thisDist = nullptr; // deallocate it

    using FillHolesType = itk::BinaryFillholeImageFilter<LabelImageType>;
    typename FillHolesType::Pointer fillHoles = FillHolesType::New();
    fillHoles->SetInput(boneBasin);
    fillHoles->SetForegroundValue(1);
    UpdateAndWrite(fillHoles->GetOutput(), boneFilename + "-basin-label.nrrd", true, 2);
    boneBasin = fillHoles->GetOutput();
    boneBasin->DisconnectPipeline();

    constexpr typename ImageType::PixelType background = -4096;

    typename ImageType::Pointer partialInput = ImageType::New();
    partialInput->CopyInformation(inImage);
    partialInput->SetRegions(safeBoneRegion);
    partialInput->Allocate(false);
    partialInput->FillBuffer(background);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [partialInput, inImage, boneBasin](const RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> tIt(boneBasin, region);
        itk::ImageRegionConstIterator<ImageType>      iIt(inImage, region);
        itk::ImageRegionIterator<ImageType>           oIt(partialInput, region);
        for (; !oIt.IsAtEnd(); ++iIt, ++tIt, ++oIt)
        {
          if (tIt.Get())
          {
            oIt.Set(iIt.Get());
          }
        }
      },
      nullptr);
    UpdateAndWrite(partialInput, boneFilename + ".nrrd", true, 1);

    using ConnectedFilterType = itk::NeighborhoodConnectedImageFilter<ImageType, LabelImageType>;
    typename ConnectedFilterType::Pointer neighborhoodConnected = ConnectedFilterType::New();
    neighborhoodConnected->SetInput(partialInput);
    neighborhoodConnected->SetLower(1500); // use a lower threshold here, so we capture more of trabecular bone
    itk::ImageRegionConstIteratorWithIndex<LabelImageType> bIt(bones, boneRegion);
    itk::ImageRegionConstIterator<LabelImageType>          bbIt(boneBasin, boneRegion);
    for (; !bIt.IsAtEnd(); ++bIt, ++bbIt)
    {
      unsigned char b = bIt.Get();
      if (b > 0)
      {
        if (b == bone)
        {
          neighborhoodConnected->AddSeed(bIt.GetIndex());
        }
        else // b != bone
        {
          if (bbIt.Get()) // this was a hole inside this bone basin
          {
            replacedBy[b] = bone; // mark it for skipping
          }
        }
      }
    }
    UpdateAndWrite(neighborhoodConnected->GetOutput(), boneFilename + "-trabecularSmall-label.nrrd", true, 3);
    typename LabelImageType::Pointer thBone = neighborhoodConnected->GetOutput();
    partialInput = nullptr; // deallocate it

    thBone = zeroPad(thBone, opSize, boneFilename + "-trabecularPadded-label.nrrd", 2);
    typename LabelImageType::Pointer dilatedBone =
      sdfDilate(thBone, 3.0 * corticalBoneThickness, boneFilename + "-trabecular1", 2);
    typename LabelImageType::Pointer erodedBone =
      sdfErode(dilatedBone, 4.0 * corticalBoneThickness, boneFilename + "-trabecular2", 3);
    dilatedBone = sdfDilate(erodedBone, 1.0 * corticalBoneThickness, boneFilename + "-trabecular3", 3);

    // now do the same for marrow, seeding from cortical and trabecular bone
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [thBone, erodedBone](const RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> bIt(erodedBone, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(thBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++oIt)
        {
          oIt.Set(bIt.Get() || oIt.Get());
        }
      },
      nullptr);
    erodedBone = nullptr; // deallocate it
    typename LabelImageType::Pointer dilatedMarrow =
      sdfDilate(thBone, 5.0 * corticalBoneThickness, boneFilename + "-marrow", 3);
    thBone = nullptr; // deallocate it
    typename LabelImageType::Pointer erodedMarrow =
      sdfErode(dilatedMarrow, 6.0 * corticalBoneThickness, boneFilename + "-marrow", 3);
    dilatedMarrow = nullptr; // deallocate it

    // now combine them, clipping them to the boneBasin
    mt->ParallelizeImageRegion<Dimension>(
      safeBoneRegion,
      [splitBones, finalBones, erodedMarrow, dilatedBone, cortexLabel, boneBasin, bone, background](
        const RegionType region) {
        itk::ImageRegionConstIterator<LabelImageType> mIt(erodedMarrow, region);
        itk::ImageRegionConstIterator<LabelImageType> bIt(dilatedBone, region);
        itk::ImageRegionConstIterator<LabelImageType> cIt(cortexLabel, region);
        itk::ImageRegionConstIterator<LabelImageType> iIt(boneBasin, region);
        itk::ImageRegionIterator<LabelImageType>      oIt(finalBones, region);
        itk::ImageRegionIterator<LabelImageType>      sIt(splitBones, region);
        for (; !oIt.IsAtEnd(); ++mIt, ++bIt, ++cIt, ++iIt, ++oIt, ++sIt)
        {
          if (iIt.Get())
          {
            if (cIt.Get())
            {
              sIt.Set(3 * bone - 2);
            }
            else if (bIt.Get())
            {
              sIt.Set(3 * bone - 1);
            }
            else if (mIt.Get())
            {
              sIt.Set(3 * bone);
            }

            if (cIt.Get() || bIt.Get() || mIt.Get())
            {
              oIt.Set(bone);
            }
          }
          // else this is background
        }
      },
      nullptr);
    UpdateAndWrite(finalBones, outFilename + "-label.nrrd", true, 0); // checkpoint write
    if (bone == 1)
    {
      UpdateAndWrite(finalBones, outFilename + "-femur-label.nrrd", true, 0);
    }
  }
  UpdateAndWrite(splitBones, outFilename + "-split-label.nrrd", true, 0);
}

int
main(int argc, char * argv[])
{
  if (argc < 3)
  {
    std::cerr << "Usage: " << std::endl;
    std::cerr << argv[0];
    std::cerr << " <InputFileName> <OutputFileName> [corticalBoneThickness] [debugLevel] [boneCount]";
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    std::string inputFileName = argv[1];
    std::string outputFileName = argv[2];
    double      corticalBoneThickness = 0.1;
    unsigned    boneCount = 255; // all bones by default
    if (argc > 3)
    {
      corticalBoneThickness = std::stod(argv[3]);
    }
    if (argc > 4)
    {
      runDebugLevel = std::stoul(argv[4]);
    }
    if (argc > 5)
    {
      boneCount = std::stoul(argv[5]);
    }

    std::cout.precision(4);
    std::cout << " InputFilePath: " << inputFileName << std::endl;
    std::cout << "OutputFileBase: " << outputFileName << std::endl;
    std::cout << "Cortical Bone Thickness: " << corticalBoneThickness << std::endl;
    std::cout << std::endl;

    constexpr unsigned ImageDimension = 3;
    using InputPixelType = short;
    using InputImageType = itk::Image<InputPixelType, ImageDimension>;

    typename InputImageType::Pointer image = itk::ReadImage<InputImageType>(inputFileName);

    using MedianType = itk::MedianImageFilter<InputImageType, InputImageType>;
    MedianType::Pointer median = MedianType::New();
    median->SetInput(image);
    UpdateAndWrite(median->GetOutput(), outputFileName + "-median.nrrd", false, 2);
    image = median->GetOutput();
    image->DisconnectPipeline();

    mainProcessing<InputImageType>(image, outputFileName, corticalBoneThickness, boneCount);
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
