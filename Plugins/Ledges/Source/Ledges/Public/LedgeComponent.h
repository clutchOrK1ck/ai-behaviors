// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "LedgeComponent.generated.h"

class ULedgeComponent;

/**
 * represents a location along the ledge
 */
struct FLedgeLocation
{
	double Distance;	// distance along the ledge from its starting point
	int SegmentIdx;		// index of the segment to which this location belongs
};

/**
 * represents a linear segment of the ledge (in the world space)
 */
struct FLedgeSegment
{
	friend class ULedgeComponent;
	
	const FVector& GetStart() const
	{
		return Start;
	}
	
	const FVector& GetEnd() const
	{
		return End;
	}
	
	const FVector& GetOrientation() const
	{
		return Orientation;
	}

private:
	FVector Start;
	FVector End;
	FVector Orientation;

	FLedgeSegment(const FVector& Start, const FVector& End, const FVector& Orientation)
		: Start(Start), End(End), Orientation(Orientation)
	{
	}
};

/**
 * represents a climbamble ledge
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LEDGES_API ULedgeComponent : public USceneComponent
{
	GENERATED_BODY()

	// control points of the ledge, in local space
	UPROPERTY()
	TArray<FVector> ControlPoints;

	// flip orientation of the ledge "faces"
	UPROPERTY()
	bool FlipOrientation;

public:
	// Sets default values for this component's properties
	ULedgeComponent();

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere)
	float LedgeWallHeight;

	UPROPERTY(EditAnywhere)
	float LedgeDepth;

	UPROPERTY(EditAnywhere)
	float LineThickness;

	UPROPERTY(EditAnywhere)
	FLinearColor LineColor;
#endif

	/**
	 * a ledge is closed if its last control points is at the same location as its first control point
	 * @return whether the ledge is closed
	 */
	bool IsClosed() const
	{
		// a closed loop must contain a minimum of three points
		return ControlPoints.Num() > 2 && ControlPoints[0].Equals(ControlPoints.Last());
	}

	/**
	 * 
	 * @param Location location along the ledge
	 * @return normal of the ledge at the specified location
	 */
	FVector GetNormal(const FLedgeLocation& Location) const;

	/**
	 * 
	 * @param SegIdx index of the segment for which to find the normal
	 * @return normal of the segment with the specified index
	 */
	FVector GetNormal(const int SegIdx) const;

	/**
	 * 
	 * @param Location location along the ledge
	 * @return whether this location is valid for this ledge
	 */
	bool IsValidLocation(const FLedgeLocation& Location) const;

	/**
	 * 
	 * @return length of this ledge in cm
	 */
	float GetLength() const;

	/**
	 * 
	 * @param Location evaluates world location at the given ledge location
	 * @return 
	 */
	FVector EvaluateWorldLocation(const FLedgeLocation& Location) const;

	/**
	 * initializes the ledge data from a spline component
	 * 
	 * @param Spline the spline component which delivers the data
	 * @param MaxSquareError the maximum square distance from the spline, 25 cm^2 by default
	 * @return initialization from spline was successful or not
	 */
	bool InitializeFromSpline(const USplineComponent* Spline, const float MaxSquareError = 25.);

	void GetSegments(TArray<FLedgeSegment>& Segments) const;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};
