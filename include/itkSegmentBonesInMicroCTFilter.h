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
#ifndef itkSegmentBonesInMicroCTFilter_h
#define itkSegmentBonesInMicroCTFilter_h

#include "itkImageToImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"


namespace itk
{

/** \class SegmentBonesInMicroCTFilter
 *
 * \brief Segments bones in microCT image. Developed for mouse knees.
 *
 * Each bone consistes of 3 labels:
 * 3n - cortical bone
 * 3n+1 - trabecular bone
 * 3n+2 - bone marrow
 *
 * Breakdown of each bone into these sub-regions is usually not overly accurate.
 *
 * \ingroup HASI
 */
template <typename TInputImage, typename TOutputImage>
class SegmentBonesInMicroCTFilter : public ImageToImageFilter<TInputImage, TOutputImage>
{
public:
  ITK_DISALLOW_COPY_AND_ASSIGN(SegmentBonesInMicroCTFilter);

  using InputImageType = TInputImage;
  using OutputImageType = TInputImage;
  using InputPixelType = typename InputImageType::PixelType;
  using OutputPixelType = typename OutputImageType::PixelType;

  /** Standard class typedefs. */
  using Self = SegmentBonesInMicroCTFilter<InputImageType, OutputImageType>;
  using Superclass = ImageToImageFilter<InputImageType, OutputImageType>;
  using Pointer = SmartPointer<Self>;
  using ConstPointer = SmartPointer<const Self>;

  /** Run-time type information. */
  itkTypeMacro(SegmentBonesInMicroCTFilter, ImageToImageFilter);

  /** Standard New macro. */
  itkNewMacro(Self);

  /** Approximate expected thickness of cortical bone
   * expressed in units of image spacing (usually millimeters). */
  itkGetConstMacro(CorticalBoneThickness, float);
  itkSetMacro(CorticalBoneThickness, float);

protected:
  SegmentBonesInMicroCTFilter();
  ~SegmentBonesInMicroCTFilter() override = default;

  void
  PrintSelf(std::ostream & os, Indent indent) const override;

  static constexpr unsigned int Dimension = TInputImage::ImageDimension;
  using RealImageType = Image<float, Dimension>;
  using RegionType = typename TOutputImage::RegionType;
  using IndexType = typename TOutputImage::IndexType;
  using SizeType = typename TOutputImage::SizeType;
  using FloatThresholdType = BinaryThresholdImageFilter<RealImageType, TOutputImage>;

  // split the binary mask into components and remove the small islands
  typename TOutputImage::Pointer
  ConnectedComponentAnalysis(typename TOutputImage::Pointer labelImage, IdentifierType & numberOfLabels);

  // compute Signed Distance Field of a binary image
  typename RealImageType::Pointer
  SDF(typename TOutputImage::Pointer labelImage);

  // morphological dilation by thresholding the distance field
  typename TOutputImage::Pointer
  SDFDilate(typename TOutputImage::Pointer labelImage, double radius);

  // morphological erosion by thresholding the distance field
  typename TOutputImage::Pointer
  SDFErode(typename TOutputImage::Pointer labelImage, double radius);

  // zero-pad an image
  typename TOutputImage::Pointer
  ZeroPad(typename TOutputImage::Pointer labelImage, Size<Dimension> padSize);

  void
  GenerateData() override;

private:
  float m_CorticalBoneThickness = 0.1;

#ifdef ITK_USE_CONCEPT_CHECKING
  itkConceptMacro(CTInputPixelIsSigned, (itk::Concept::Signed<typename InputImageType::PixelType>));
  itkConceptMacro(InputAndOutputMustHaveSameDimension,
                  (itk::Concept::SameDimension<TInputImage::ImageDimension, TOutputImage::ImageDimension>));
#endif
};
} // namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#  include "itkSegmentBonesInMicroCTFilter.hxx"
#endif

#endif // itkSegmentBonesInMicroCTFilter
