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
#ifndef itkSegmentBonesInMicroCTFilter_hxx
#define itkSegmentBonesInMicroCTFilter_hxx

#include "itkSegmentBonesInMicroCTFilter.h"

#include "itkArray.h"
#include "itkMedianImageFilter.h"
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

namespace itk
{
template <typename TInputImage, typename TOutputImage>
void
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
  os << indent << "CorticalBoneThickness: " << m_CorticalBoneThickness << std::endl;
}

template <typename TInputImage, typename TOutputImage>
typename TOutputImage::Pointer
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::ConnectedComponentAnalysis(
  typename TOutputImage::Pointer labelImage,
  IdentifierType &               numberOfLabels)
{
  using ManyLabelImageType = Image<SizeValueType, TOutputImage::ImageDimension>;
  using LabelerType = ConnectedComponentImageFilter<TOutputImage, ManyLabelImageType>;
  typename LabelerType::Pointer labeler = LabelerType::New();
  labeler->SetInput(labelImage);

  using RelabelType = RelabelComponentImageFilter<ManyLabelImageType, TOutputImage>;
  typename RelabelType::Pointer relabeler = RelabelType::New();
  relabeler->SetInput(labeler->GetOutput());
  relabeler->SetMinimumObjectSize(1000);

  relabeler->Update();
  numberOfLabels = relabeler->GetNumberOfObjects();
  return relabeler->GetOutput();
}

template <typename TInputImage, typename TOutputImage>
typename SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::RealImageType::Pointer
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::SDF(typename TOutputImage::Pointer labelImage)
{
  using DistanceFieldType = SignedMaurerDistanceMapImageFilter<TOutputImage, RealImageType>;
  typename DistanceFieldType::Pointer distF = DistanceFieldType::New();
  distF->SetInput(labelImage);
  distF->SetSquaredDistance(true);
  distF->Update();
  typename RealImageType::Pointer dist = distF->GetOutput();
  dist->DisconnectPipeline();
  return dist;
}

template <typename TInputImage, typename TOutputImage>
typename TOutputImage::Pointer
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::SDFDilate(typename TOutputImage::Pointer labelImage,
                                                                  double                         radius)
{
  typename FloatThresholdType::Pointer sdfTh = FloatThresholdType::New();
  sdfTh->SetInput(this->SDF(labelImage));
  sdfTh->SetUpperThreshold(radius * radius);
  sdfTh->Update();
  return sdfTh->GetOutput();
}

template <typename TInputImage, typename TOutputImage>
typename TOutputImage::Pointer
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::SDFErode(typename TOutputImage::Pointer labelImage,
                                                                 double                         radius)
{
  // we need an inversion filter because Maurer's filter distances are not symmetrical
  // inside distances start at 0, while outside distances start at single spacing
  using NotType = NotImageFilter<TOutputImage, TOutputImage>;
  typename NotType::Pointer negator = NotType::New();
  negator->SetInput(labelImage);
  negator->Update();

  typename FloatThresholdType::Pointer sdfTh = FloatThresholdType::New();
  sdfTh->SetInput(this->SDF(negator->GetOutput()));
  sdfTh->SetLowerThreshold(radius * radius);
  sdfTh->Update();

  return sdfTh->GetOutput();
}

template <typename TInputImage, typename TOutputImage>
typename TOutputImage::Pointer
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::ZeroPad(typename TOutputImage::Pointer labelImage,
                                                                Size<Dimension>                padSize)
{
  using PadType = ConstantPadImageFilter<TOutputImage, TOutputImage>;
  typename PadType::Pointer padder = PadType::New();
  padder->SetInput(labelImage);
  padder->SetPadBound(padSize);
  padder->Update();
  return padder->GetOutput();
}


template <typename TInputImage, typename TOutputImage>
void
SegmentBonesInMicroCTFilter<TInputImage, TOutputImage>::GenerateData()
{
  this->AllocateOutputs();

  typename TInputImage::ConstPointer inImage = this->GetInput();

  Array<double> sigmaArray(1);
  sigmaArray[0] = m_CorticalBoneThickness;
  using BinaryThresholdType = BinaryThresholdImageFilter<TInputImage, TOutputImage>;

  const double maxRadius = 8.0 * m_CorticalBoneThickness; // allow some room for imperfect intermediate steps
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
  MultiThreaderBase::Pointer mt = MultiThreaderBase::New();

  typename TOutputImage::Pointer gaussLabel;
  {
    using GaussType = SmoothingRecursiveGaussianImageFilter<TInputImage>;
    typename GaussType::Pointer gaussF = GaussType::New();
    gaussF->SetInput(inImage);
    gaussF->SetSigma(m_CorticalBoneThickness);
    gaussF->Update();

    typename BinaryThresholdType::Pointer binTh2 = BinaryThresholdType::New();
    binTh2->SetInput(gaussF->GetOutput());
    binTh2->SetLowerThreshold(2000);
    binTh2->Update();
    gaussLabel = binTh2->GetOutput();
  }

  // Create a process accumulator for tracking the progress of minipipeline
  ProgressAccumulator::Pointer progress = ProgressAccumulator::New();
  progress->SetMiniPipelineFilter(this);

  typename TOutputImage::Pointer descoLabel;
  {
    using MultiScaleHessianFilterType = MultiScaleHessianEnhancementImageFilter<TInputImage, RealImageType>;
    using EigenValueImageType = typename MultiScaleHessianFilterType::EigenValueImageType;
    using DescoteauxEigenToScalarImageFilterType =
      DescoteauxEigenToMeasureImageFilter<EigenValueImageType, RealImageType>;
    using DescoteauxMeasureEstimationType = DescoteauxEigenToMeasureParameterEstimationFilter<EigenValueImageType>;

    typename MultiScaleHessianFilterType::Pointer multiScaleFilter = MultiScaleHessianFilterType::New();
    multiScaleFilter->SetInput(inImage);
    multiScaleFilter->SetSigmaArray(sigmaArray);
    typename DescoteauxEigenToScalarImageFilterType::Pointer descoFilter =
      DescoteauxEigenToScalarImageFilterType::New();
    multiScaleFilter->SetEigenToMeasureImageFilter(descoFilter);
    typename DescoteauxMeasureEstimationType::Pointer descoEstimator = DescoteauxMeasureEstimationType::New();
    multiScaleFilter->SetEigenToMeasureParameterEstimationFilter(descoEstimator);
    progress->RegisterInternalFilter(multiScaleFilter, 0.5f);
    multiScaleFilter->Update();

    typename FloatThresholdType::Pointer descoTh = FloatThresholdType::New();
    descoTh->SetInput(multiScaleFilter->GetOutput());
    descoTh->SetLowerThreshold(0.1);
    descoTh->Update();
    descoLabel = descoTh->GetOutput();
    this->UpdateProgress(0.51f);
  }


  typename BinaryThresholdType::Pointer binTh = BinaryThresholdType::New();
  binTh->SetInput(inImage);
  binTh->SetLowerThreshold(5000); // start from a high threshold, so bones are well separated
  binTh->Update();
  typename TOutputImage::Pointer thLabel = binTh->GetOutput();

  // create cortexLabel with information from descoLabel, gaussLabel and thLabel
  typename TOutputImage::Pointer cortexLabel = TOutputImage::New();
  cortexLabel->CopyInformation(inImage);
  cortexLabel->SetRegions(paddedWholeImage);
  cortexLabel->Allocate(true);
  mt->ParallelizeImageRegion<Dimension>(
    wholeImage,
    [descoLabel, gaussLabel, thLabel, cortexLabel](const RegionType region) {
      ImageRegionConstIterator<TOutputImage> gIt(gaussLabel, region);
      ImageRegionConstIterator<TOutputImage> tIt(thLabel, region);
      ImageRegionConstIterator<TOutputImage> dIt(descoLabel, region);
      ImageRegionIterator<TOutputImage>      cIt(cortexLabel, region);
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
  typename TOutputImage::Pointer cortexEroded = this->SDFErode(cortexLabel, 0.5 * m_CorticalBoneThickness);
  descoLabel = nullptr; // deallocate it
  gaussLabel = nullptr; // deallocate it
  this->UpdateProgress(0.52f);

  typename TOutputImage::Pointer finalBones = this->GetOutput();

  // do morphological processing per bone, to avoid merging bones which are close to each other
  IdentifierType numBones = 0;

  typename TOutputImage::Pointer bones = this->ConnectedComponentAnalysis(thLabel, numBones);
  // we might not even get to this point if there are more than 255 bones
  // we need 3 labels per bone, one each for cortical, trabecular and marrow
  itkAssertOrThrowMacro(numBones <= 85, "There are too many bones to fit into uchar");
  this->UpdateProgress(0.55f);

  bones = this->ZeroPad(bones, opSize);
  this->UpdateProgress(0.56f);
  typename RealImageType::Pointer boneDist = this->SDF(bones);
  this->UpdateProgress(0.69f);

  // calculate bounding box for each bone
  std::vector<IndexType> minIndices(numBones + 1, IndexType::Filled(NumericTraits<IndexValueType>::max()));
  std::vector<IndexType> maxIndices(numBones + 1, IndexType::Filled(NumericTraits<IndexValueType>::NonpositiveMin()));
  std::vector<unsigned char> replacedBy(numBones + 1, 0);
  {
    ImageRegionConstIteratorWithIndex<TOutputImage> bIt(bones, wholeImage);
    ImageRegionConstIterator<TOutputImage>          cIt(cortexEroded, wholeImage);
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
  this->UpdateProgress(0.7f);


  // per-bone processing
  for (unsigned bone = 1; bone <= numBones; bone++)
  {
    float boneProgress = 0.3f / numBones;
    float beginProgress = 0.7 + boneProgress * (bone - 1);
    this->UpdateProgress(beginProgress + boneProgress * 0.0f);

    if (replacedBy[bone] > 0)
    {
      std::cout << "Bone " << bone << " was an island inside bone " << unsigned(replacedBy[bone]) << std::endl;
      continue; // next bone
    }

    // calculate expanded bounding box, so the subsequent operations don't need to process the whole image
    RegionType boneRegion;         // tight bounding box
    RegionType expandedBoneRegion; // bounding box with room for morphological operations
    for (unsigned d = 0; d < Dimension; d++)
    {
      boneRegion.SetIndex(d, minIndices[bone][d]);
      boneRegion.SetSize(d, maxIndices[bone][d] - minIndices[bone][d] + 1);

      IndexValueType opSizeD = opSize[d]; // a signed value
      expandedBoneRegion.SetIndex(d, minIndices[bone][d] - opSizeD);
      expandedBoneRegion.SetSize(d, maxIndices[bone][d] - minIndices[bone][d] + 1 + 2 * opSizeD);
    }
    RegionType safeBoneRegion = expandedBoneRegion;
    safeBoneRegion.Crop(wholeImage); // restrict to image size

    typename TOutputImage::Pointer thisBone = TOutputImage::New();
    thisBone->CopyInformation(inImage);
    thisBone->SetRegions(expandedBoneRegion);
    thisBone->Allocate(true);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [thisBone, bones, bone](const RegionType region) {
        ImageRegionConstIterator<TOutputImage> bIt(bones, region);
        ImageRegionIterator<TOutputImage>      oIt(thisBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++oIt)
        {
          if (bIt.Get() == bone)
          {
            oIt.Set(bone);
          }
        }
      },
      nullptr);
    typename RealImageType::Pointer thisDist = this->SDF(thisBone);
    thisBone = nullptr; // deallocate it
    this->UpdateProgress(beginProgress + boneProgress * 0.05f);

    typename TOutputImage::Pointer boneBasin = TOutputImage::New();
    boneBasin->CopyInformation(inImage);
    boneBasin->SetRegions(safeBoneRegion);
    boneBasin->Allocate(true);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [boneBasin, thisDist, boneDist, epsDist](const RegionType region) {
        ImageRegionConstIterator<RealImageType> tIt(thisDist, region);
        ImageRegionConstIterator<RealImageType> gIt(boneDist, region);
        ImageRegionIterator<TOutputImage>       oIt(boneBasin, region);
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
    this->UpdateProgress(beginProgress + boneProgress * 0.10f);

    using FillHolesType = BinaryFillholeImageFilter<TOutputImage>;
    typename FillHolesType::Pointer fillHoles = FillHolesType::New();
    fillHoles->SetInput(boneBasin);
    fillHoles->SetForegroundValue(1);
    fillHoles->Update();
    boneBasin = fillHoles->GetOutput();
    boneBasin->DisconnectPipeline();
    this->UpdateProgress(beginProgress + boneProgress * 0.20f);

    constexpr typename TInputImage::PixelType background = -4096;

    typename TInputImage::Pointer partialInput = TInputImage::New();
    partialInput->CopyInformation(inImage);
    partialInput->SetRegions(safeBoneRegion);
    partialInput->Allocate(false);
    partialInput->FillBuffer(background);
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [partialInput, inImage, boneBasin](const RegionType region) {
        ImageRegionConstIterator<TOutputImage> tIt(boneBasin, region);
        ImageRegionConstIterator<TInputImage>  iIt(inImage, region);
        ImageRegionIterator<TInputImage>       oIt(partialInput, region);
        for (; !oIt.IsAtEnd(); ++iIt, ++tIt, ++oIt)
        {
          if (tIt.Get())
          {
            oIt.Set(iIt.Get());
          }
        }
      },
      nullptr);
    this->UpdateProgress(beginProgress + boneProgress * 0.25f);

    using ConnectedFilterType = NeighborhoodConnectedImageFilter<TInputImage, TOutputImage>;
    typename ConnectedFilterType::Pointer neighborhoodConnected = ConnectedFilterType::New();
    neighborhoodConnected->SetInput(partialInput);
    neighborhoodConnected->SetLower(1500); // use a lower threshold here, so we capture more of trabecular bone
    ImageRegionConstIteratorWithIndex<TOutputImage> bIt(bones, boneRegion);
    ImageRegionConstIterator<TOutputImage>          bbIt(boneBasin, boneRegion);
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
    neighborhoodConnected->Update();
    typename TOutputImage::Pointer thBone = neighborhoodConnected->GetOutput();
    partialInput = nullptr; // deallocate it
    this->UpdateProgress(beginProgress + boneProgress * 0.35f);

    thBone = this->ZeroPad(thBone, opSize);
    this->UpdateProgress(beginProgress + boneProgress * 0.40f);
    typename TOutputImage::Pointer dilatedBone = this->SDFDilate(thBone, 3.0 * m_CorticalBoneThickness);
    this->UpdateProgress(beginProgress + boneProgress * 0.50f);
    typename TOutputImage::Pointer erodedBone = this->SDFErode(dilatedBone, 4.0 * m_CorticalBoneThickness);
    this->UpdateProgress(beginProgress + boneProgress * 0.60f);
    dilatedBone = this->SDFDilate(erodedBone, 1.0 * m_CorticalBoneThickness);
    this->UpdateProgress(beginProgress + boneProgress * 0.70f);

    // now do the same for marrow, seeding from cortical and trabecular bone
    mt->ParallelizeImageRegion<Dimension>(
      boneRegion,
      [thBone, erodedBone](const RegionType region) {
        ImageRegionConstIterator<TOutputImage> bIt(erodedBone, region);
        ImageRegionIterator<TOutputImage>      oIt(thBone, region);
        for (; !oIt.IsAtEnd(); ++bIt, ++oIt)
        {
          oIt.Set(bIt.Get() || oIt.Get());
        }
      },
      nullptr);
    erodedBone = nullptr; // deallocate it
    this->UpdateProgress(beginProgress + boneProgress * 0.75f);
    typename TOutputImage::Pointer dilatedMarrow = this->SDFDilate(thBone, 5.0 * m_CorticalBoneThickness);
    thBone = nullptr; // deallocate it
    this->UpdateProgress(beginProgress + boneProgress * 0.85f);
    typename TOutputImage::Pointer erodedMarrow = this->SDFErode(dilatedMarrow, 6.0 * m_CorticalBoneThickness);
    dilatedMarrow = nullptr; // deallocate it
    this->UpdateProgress(beginProgress + boneProgress * 0.95f);


    // now combine them, clipping them to the boneBasin
    mt->ParallelizeImageRegion<Dimension>(
      safeBoneRegion,
      [finalBones, erodedMarrow, dilatedBone, cortexLabel, boneBasin, bone, this](const RegionType region) {
        ImageRegionConstIterator<TOutputImage> mIt(erodedMarrow, region);
        ImageRegionConstIterator<TOutputImage> bIt(dilatedBone, region);
        ImageRegionConstIterator<TOutputImage> cIt(cortexLabel, region);
        ImageRegionConstIterator<TOutputImage> iIt(boneBasin, region);
        ImageRegionIterator<TOutputImage>      oIt(finalBones, region);
        for (; !oIt.IsAtEnd(); ++mIt, ++bIt, ++cIt, ++iIt, ++oIt)
        {
          if (iIt.Get())
          {
            if (this->m_WholeBones)
            {
              if (cIt.Get() || bIt.Get() || mIt.Get())
              {
                oIt.Set(bone);
              }
            }
            else // split bones
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
          }
          // else this is background
        }
      },
      nullptr);
  }
  this->UpdateProgress(1.0f);
}

} // end namespace itk

#endif // itkSegmentBonesInMicroCTFilter_hxx
