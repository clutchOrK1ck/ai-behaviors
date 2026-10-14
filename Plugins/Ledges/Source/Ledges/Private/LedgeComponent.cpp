#include "LedgeComponent.h"
#include "SegmentTypes.h"

ULedgeComponent::ULedgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	FlipOrientation = false;
}

FVector ULedgeComponent::GetNormal(const FLedgeLocation& Location) const
{
	return GetNormal(Location.SegmentIdx);
}

FVector ULedgeComponent::GetNormal(const int SegIdx) const
{
	if (!ControlPoints.IsValidIndex(SegIdx + 1))
	{
		return FVector::ZeroVector;
	}

	const auto Segment = UE::Geometry::FSegment3d(ControlPoints[SegIdx], ControlPoints[SegIdx + 1]);

	if (FlipOrientation)
	{
		return Segment.Direction.Cross(FVector::UpVector * -1.f).GetSafeNormal();
	}

	return Segment.Direction.Cross(FVector::UpVector).GetSafeNormal();
}

bool ULedgeComponent::IsValidLocation(const FLedgeLocation& Location) const
{
	return Location.Distance <= GetLength() && ControlPoints.IsValidIndex(Location.SegmentIdx + 1);
}

float ULedgeComponent::GetLength() const
{
	float Length = 0.;
	
	if (ControlPoints.Num() < 2)
	{
		return Length;
	}

	for (int i = 1; i < ControlPoints.Num(); i++)
	{
		Length += (ControlPoints[i] - ControlPoints[i-1]).Length();
	}

	return Length;
}

void ULedgeComponent::GetSegments(TArray<FLedgeSegment>& Segments) const
{
	for (int i = 1; i < ControlPoints.Num(); i++)
	{
		Segments.Add(FLedgeSegment{
			GetComponentTransform().TransformPosition(ControlPoints[0]),
			GetComponentTransform().TransformPosition(ControlPoints[1]),
			GetNormal(i - 1)
		});
	}
}

FBoxSphereBounds ULedgeComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (ControlPoints.IsEmpty())
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.);
	}

	return FBoxSphereBounds(FBox(ControlPoints).TransformBy(LocalToWorld));
}

FVector ULedgeComponent::EvaluateWorldLocation(const FLedgeLocation& Location) const
{
	if (!IsValidLocation(Location))
	{
		return FVector::ZeroVector;
	}
	
	// TODO this is pretty damn inefficient, especially if it's going to be called every frame
	float RemainingLength = Location.Distance;
	FVector LocationLocal;
	for (int i = 1; i < ControlPoints.Num(); i++)
	{
		const auto SegmentLength = (ControlPoints[i] - ControlPoints[i - 1]).Length();
		
		if (SegmentLength <= Location.Distance)
		{
			LocationLocal = (ControlPoints[i] - ControlPoints[i-1]) * SegmentLength / Location.Distance + ControlPoints[i - 1];
			break;
		}

		RemainingLength -= SegmentLength;
	}

	return GetComponentTransform().TransformPosition(LocationLocal);
}

bool ULedgeComponent::InitializeFromSpline(const USplineComponent* Spline, const float MaxSquareError)
{
	if (FMath::IsNearlyZero(Spline->GetSplineLength()))
	{
		return false;
	}

	TArray<FVector> SplinePolyLine;
	if (!Spline->ConvertSplineToPolyLine(ESplineCoordinateSpace::World, MaxSquareError, SplinePolyLine))
	{
		return false;
	}

	ControlPoints.Empty();
	for (const auto& SplinePoint : SplinePolyLine)
	{
		ControlPoints.Add(GetComponentTransform().InverseTransformPosition(SplinePoint));
	}

	// TODO flip orientation?

	return true;
}
