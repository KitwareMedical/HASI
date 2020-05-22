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
#ifndef itkLandmarkAtlasSegmentationFilter_h
#define itkLandmarkAtlasSegmentationFilter_h

#include "itkImageToImageFilter.h"
#include "itkPointSet.h"

namespace itk
{

/** \class LandmarkAtlasSegmentationFilter
 *
 * \brief Filters a image by iterating over its pixels.
 *
 * Filters a image by iterating over its pixels in a multi-threaded way
 * and {to be completed by the developer}.
 *
 * \ingroup HASI
 *
 */
template <typename TInputImage, typename TOutputImage>
class LandmarkAtlasSegmentationFilter : public ImageToImageFilter<TInputImage, TOutputImage>
{
public:
  ITK_DISALLOW_COPY_AND_ASSIGN(LandmarkAtlasSegmentationFilter);

  static constexpr unsigned Dimension = TInputImage::ImageDimension;

  using InputImageType = TInputImage;
  using OutputImageType = TOutputImage;
  using InputPixelType = typename InputImageType::PixelType;
  using OutputPixelType = typename OutputImageType::PixelType;

  /** Standard class typedefs. */
  using Self = LandmarkAtlasSegmentationFilter<InputImageType, OutputImageType>;
  using Superclass = ImageToImageFilter<InputImageType, OutputImageType>;
  using Pointer = SmartPointer<Self>;
  using ConstPointer = SmartPointer<const Self>;

  /** Run-time type information. */
  itkTypeMacro(LandmarkAtlasSegmentationFilter, ImageToImageFilter);

  /** Standard New macro. */
  itkNewMacro(Self);

  using PointSetType = PointSet<double, Dimension>;

  /** Get/Set the atlas pointset.  */
  itkSetConstObjectMacro(AtlasLandmarks, PointSetType);
  itkGetConstObjectMacro(AtlasLandmarks, PointSetType);

  /** Get/Set the input point set.  */
  itkSetConstObjectMacro(InputLandmarks, PointSetType);
  itkGetConstObjectMacro(InputLandmarks, PointSetType);


protected:
  LandmarkAtlasSegmentationFilter()
  {
    Self::SetPrimaryInputName("InputImage"); // 0 (primary) is already required
    Self::AddRequiredInputName("AtlasImage", 1);
    Self::AddRequiredInputName("AtlasLabels", 2);
    Self::AddRequiredInputName("InputLandmarks", 3);
    Self::AddRequiredInputName("AtlasLandmarks", 4);
  }
  ~LandmarkAtlasSegmentationFilter() override = default;

  void
  PrintSelf(std::ostream & os, Indent indent) const override;

  using RealImageType = itk::Image<float, 3>;
  using RegionType = typename TOutputImage::RegionType;
  using IndexType = typename TOutputImage::IndexType;
  using SizeType = typename TOutputImage::SizeType;
  using PointType = typename TOutputImage::PointType;

  void
  GenerateData() override;

private:
  PointSetType m_AtlasLandmarks = nullptr;
  PointSetType m_InputLandmarks = nullptr;

#ifdef ITK_USE_CONCEPT_CHECKING
  itkConceptMacro(InputAndOutputMustHaveSameDimension,
                  (itk::Concept::SameDimension<TInputImage::ImageDimension, TOutputImage::ImageDimension>));
#endif
};
} // namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#  include "itkLandmarkAtlasSegmentationFilter.hxx"
#endif

#endif // itkLandmarkAtlasSegmentationFilter
