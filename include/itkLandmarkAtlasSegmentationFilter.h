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
#include "itkVersorRigid3DTransform.h"
#include "itkAffineTransform.h"
#include "itkCompositeTransform.h"


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


  /** Get/Set the input labels.
   * This is basic segmentation into bones,
   * or cortical, trabecular and marrow. */
  itkSetObjectMacro(InputLabels, TOutputImage);
  itkGetModifiableObjectMacro(InputLabels, TOutputImage);

  /** Get/Set the atlas labels. */
  itkSetObjectMacro(AtlasLabels, TOutputImage);
  itkGetModifiableObjectMacro(AtlasLabels, TOutputImage);

  using PointType = typename TOutputImage::PointType;
  using LandmarksType = std::vector<PointType>;

  /** Get/Set the input landmarks. */
  virtual void
  SetInputLandmarks(const LandmarksType landmarks)
  {
    if (this->m_InputLandmarks != landmarks)
    {
      this->m_InputLandmarks = landmarks;
      this->Modified();
    }
  }
  itkGetConstReferenceMacro(InputLandmarks, LandmarksType);

  /** Get/Set the atlas landmarks. */
  virtual void
  SetAtlasLandmarks(const LandmarksType landmarks)
  {
    if (this->m_AtlasLandmarks != landmarks)
    {
      this->m_AtlasLandmarks = landmarks;
      this->Modified();
    }
  }
  itkGetConstReferenceMacro(AtlasLandmarks, LandmarksType);

  using RigidTransformType = itk::VersorRigid3DTransform<double>;

  /** Only valid after Update. */
  itkGetConstObjectMacro(LandmarksTransform, RigidTransformType);

  /** Only valid after Update. */
  itkGetConstObjectMacro(RigidTransform, RigidTransformType);

  using AffineTransformType = itk::AffineTransform<double, Dimension>;

  /** Only valid after Update. */
  itkGetConstObjectMacro(AffineTransform, AffineTransformType);

  using CompositeTransformType = itk::CompositeTransform<>;

  /** Only valid after Update. */
  itkGetConstObjectMacro(FinalTransform, CompositeTransformType);

  /** Should the registration stop at affine transform?
   * If false, BSpline transform is computed. */
  itkSetMacro(StopAtAffine, bool);
  itkGetMacro(StopAtAffine, bool);
  itkBooleanMacro(StopAtAffine);

protected:
  LandmarkAtlasSegmentationFilter()
  {
    this->SetNumberOfRequiredInputs(2);
    Self::SetPrimaryInputName("InputImage");
    Self::AddRequiredInputName("AtlasImage", 1);
  }
  ~LandmarkAtlasSegmentationFilter() override = default;

  void
  PrintSelf(std::ostream & os, Indent indent) const override;

  // we need to override this in order to allow
  // the inputs to cover different physical space
  void
  VerifyInputInformation() const override
  {}

  using RealImageType = Image<float, Dimension>;
  using RegionType = typename TOutputImage::RegionType;
  using IndexType = typename TOutputImage::IndexType;
  using SizeType = typename TOutputImage::SizeType;

  template <typename TImage>
  typename TImage::Pointer
  Duplicate(const TImage * input, const RegionType subRegion);

  void
  AffineFromRigid();

  void
  GenerateData() override;

private:
  typename TOutputImage::Pointer m_InputLabels = nullptr;
  typename TOutputImage::Pointer m_AtlasLabels = nullptr;

  LandmarksType m_AtlasLandmarks;
  LandmarksType m_InputLandmarks;

  typename RigidTransformType::Pointer     m_LandmarksTransform = nullptr;
  typename RigidTransformType::Pointer     m_RigidTransform = nullptr;
  typename AffineTransformType::Pointer    m_AffineTransform = nullptr;
  typename CompositeTransformType::Pointer m_FinalTransform = nullptr;

  bool m_StopAtAffine = true;

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
