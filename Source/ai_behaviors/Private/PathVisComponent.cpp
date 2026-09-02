// Fill out your copyright notice in the Description page of Project Settings.


#include "PathVisComponent.h"

#include "NavigationSystem.h"
#include "NavLinkCustomInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"


struct FPolyLine
{
	TArray<FVector> Points;

	/**
	 * distributes points on this poly line
	 * @param Distance distance between points
	 * @param OmitEnds do not generate points in the beginning and in the end
	 * @param OutPoints 
	 */
	void DistributePoints(float Distance, TArray<FVector>& OutPoints, bool OmitEnds = false) const
	{
		const int NumPoints = FMath::Floor(GetLength() / Distance);
		const bool bNearlyPerfectFit = FMath::IsNearlyEqual(NumPoints * Distance, GetLength(), 100.f / 2.); // consider the spare distance of interval/2 to be a near perfect fit
		
		// TODO more efficient implementation?
		for (int i = 0; i < NumPoints; i++)
		{
			if ((i == 0 && OmitEnds) ||
				(i == (NumPoints - 1) && bNearlyPerfectFit && OmitEnds))
			{
				continue;
			}
			
			OutPoints.Add(GetLocationAtDistance(i * Distance));
		}
	}

	/**
	 * tries to fit points on this poly line, respecting radii of the poly line ends (no points will spawn there)
	 * @param OutPoints 
	 * @param Distance distance between the points
	 * @param EndsRadius no points will spawn in this radius around the poly line ends
	 */
	void FitPoints(TArray<FVector>& OutPoints, float Distance, float EndsRadius) const
	{
		const float UsableLength = GetLength() - EndsRadius * 2.;
		if (UsableLength <= 0.)
		{
			return;
		}

		const int NumSegments = FMath::RoundToInt(UsableLength / Distance);
		const float SegmentLength = UsableLength / NumSegments;

		for (int i = 0; i < NumSegments; i++)
		{
			// place in the center of the segment
			OutPoints.Add(GetLocationAtDistance(EndsRadius + i * SegmentLength + SegmentLength / 2.));
		}
	}
	
	FVector GetLocationAtDistance(float Distance) const
	{
		if (Points.IsEmpty())
		{
			return FVector::ZeroVector;
		}

		if (Points.Num() == 1)
		{
			return Points[0];
		}

		auto RemainingDistance = Distance;
		if (RemainingDistance <= 0.f)
		{
			return Points[0];
		}

		for (int i = 1; i < Points.Num(); i++)
		{
			float SegmentLength = (Points[i] - Points[i-1]).Length();
			if (RemainingDistance <= SegmentLength)
			{
				return Points[i-1] + (Points[i] - Points[i-1]) * (RemainingDistance / SegmentLength);
			}
			
			RemainingDistance -= SegmentLength;
		}

		return Points.Last();
	}
	
	float GetLength() const
	{
		if (Points.Num() <= 1)
		{
			return 0.f;
		}

		float Length = 0.f;
		for (int i = 1; i < Points.Num(); i++)
		{
			Length += (Points[i] - Points[i-1]).Length();
		}

		return Length;
	}
};

void CreatePathVisPointsOnPolyline(const FPolyLine& InPolyLine,
	TArray<FPathVisPathPoint>& OutPoints,
	float Distance,
	float KeypointRadius,
	EPathVisualizationPointType Type)
{
	TArray<FVector> Locations;
	InPolyLine.FitPoints(Locations, Distance, KeypointRadius);

	for (const auto& Location: Locations)
	{
		OutPoints.Add(
			FPathVisPathPoint(Location, Type));
	}
}

void UPathVisComponent::CreatePathVisPointsFromNavPathPoints(const ACharacter* MovingChar,
	FNavMeshPath* InPath, TArray<FPathVisPathPoint>& OutPoints)
{
	// create a spline from the nav path to distribute points on it
	FPolyLine PathPolyLine;

	// add the destination to the path points at once
	if (!HasCharacterAlreadyPassedPathPoint(MovingChar, InPath, InPath->GetPathPoints().Num()))
	{
		OutPoints.Add(FPathVisPathPoint(
			*InPath->GetPathPointLocation(InPath->GetPathPoints().Num() - 1), Destination));
	} else
	{
		return;
	}
	
	// create the polyline using path's points
	for (int PointIdx = InPath->GetPathPoints().Num() - 1; PointIdx >= 0; PointIdx--)
	{
		FVector NavPathLocation = *Path->GetPathPointLocation(PointIdx);
		FNavPathPoint NavPathPoint;
		FNavigationPath::GetPathPoint(InPath, PointIdx, NavPathPoint);

		// before anything, we must check if the moving character has already passed this point
		if (HasCharacterAlreadyPassedPathPoint(MovingChar, InPath, PointIdx))
		{
			// add their current feet location to the path poly line, distribute points, and break!
			PathPolyLine.Points.Add(MovingChar->GetMovementComponent()->GetFeetLocation());
			CreatePathVisPointsOnPolyline(PathPolyLine, OutPoints, WaypointDistance, KeypointRadius, Waypoint);
			return;
		}

		PathPolyLine.Points.Add(NavPathLocation);

		// if the preceding path point is a nav link, this point must be the link destination
		if (FNavPathPoint PrecedingPoint; FNavigationPath::GetPathPoint(InPath, PointIdx - 1, PrecedingPoint) &&
			FNavMeshNodeFlags(PrecedingPoint.Flags).IsNavLink())
		{
			CreatePathVisPointsOnPolyline(PathPolyLine, OutPoints, WaypointDistance, KeypointRadius, Waypoint);

			OutPoints.Add(
				FPathVisPathPoint(NavPathLocation,
				                  GetNavLinkTypeFromNavLinkPoint(PrecedingPoint) == ENavLinkDirection::Type::BothWays
					                  ? NavlinkBi
					                  : NavlinkEnd));

			PathPolyLine.Points.Empty();
			PathPolyLine.Points.Add(NavPathLocation);
		}

		// this point is a nav link - the following point (already in the path polyline) is then the navlink destination
		if (FNavMeshNodeFlags(NavPathPoint.Flags).IsNavLink())
		{
			CreatePathVisPointsOnPolyline(PathPolyLine, OutPoints, WaypointDistance, KeypointRadius, OffMeshWaypoint);
			
			// add the point itself to the waypoints
			OutPoints.Add(FPathVisPathPoint(NavPathLocation,
				GetNavLinkTypeFromNavLinkPoint(NavPathPoint) == ENavLinkDirection::Type::BothWays
				? NavlinkBi
				: NavlinkStart));
			
			PathPolyLine.Points.Empty();
			PathPolyLine.Points.Add(NavPathLocation);
		}
	}
}

void UPathVisComponent::CreatePointsFromPathCorridor(const ACharacter* MovingChar,
	FNavMeshPath* InPath,
	TArray<FPathVisPathPoint>& OutPoints)
{
	// get the recast nav mesh
	const UNavigationSystemV1* NavSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem());
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavSystem->GetMainNavData());

	if (!NavMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("Navigation system has returned an invalid pointer to the recast nav mesh"));
		return;
	}
	
	FPolyLine PathPolyLine;
	FVector DestinationLocation = InPath->GetDestinationLocation();

	OutPoints.Add(FPathVisPathPoint(DestinationLocation, Destination));
	PathPolyLine.Points.Add(DestinationLocation);

	const FVector FeetLocation = MovingChar->GetMovementComponent()->GetFeetLocation();
	
	for (int i = InPath->PathCorridor.Num() - 1; i >= 0; i--)
	{
		const auto PolyId = InPath->PathCorridor[i];
		FVector PolyCenter;
		NavMesh->GetPolyCenter(PolyId, PolyCenter);

		// this poly is the current path target? include it and break
		if (auto CurrentPathElement = PFComponent->GetCurrentPathElement();
			InPath->PathCorridor.IsValidIndex(CurrentPathElement) && InPath->PathCorridor[CurrentPathElement] == PolyId)
		{
			// PathPolyLine.Points.Add(PolyCenter);
			break;
		}
		
		// do not include the poly center for the poly that contains the destination location
		if (NavMesh->DoesNodeContainLocation(PolyId, InPath->GetDestinationLocation()))
		{
			continue;
		}
		
		PathPolyLine.Points.Add(PolyCenter);
	}
	
	PathPolyLine.Points.Add(FeetLocation);
	
	TArray<FVector> Pts;
	PathPolyLine.FitPoints(Pts, WaypointDistance, KeypointRadius);

	for (auto Pt : Pts)
	{
		FPathVisPathPoint PathVisPt {
			Pt, Waypoint
		};
		OutPoints.Add(PathVisPt);
	}
}

bool UPathVisComponent::HasCharacterAlreadyPassedPathPoint(const ACharacter* Character, FNavMeshPath* InPath,
                                                           uint32 PointIdx)
{
	const auto PathPointLocation = *InPath->GetPathPointLocation(PointIdx);
	const auto CharacterFeetLocation = Character->GetCharacterMovement()->GetFeetLocation();
	
	return InPath->GetLengthFromPosition(CharacterFeetLocation, PFComponent->GetCurrentPathElement()) <
		InPath->GetLengthFromPosition(PathPointLocation, PointIdx);
}

ENavLinkDirection::Type UPathVisComponent::GetNavLinkTypeFromNavLinkPoint(const FNavPathPoint& Pt) const
{
	ENavLinkDirection::Type NavLinkType;
	if (const UNavigationSystemV1* NavSys = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem()))
	{
		if (const auto NavLink = NavSys->GetCustomLink(Pt.CustomNavLinkId))
		{
			FVector PlaceholderVec;
			NavLink->GetLinkData(PlaceholderVec, PlaceholderVec, NavLinkType);
		}
	}

	return NavLinkType;
}

void UPathVisComponent::UpdatePathPoints(EPathPointsUpdateReason Reason)
{
	bool bUpdatedPathPoints = false;

	if (!Path || !PFComponent || !OwnerChar || Reason == DestinationReached)
	{
		PathPoints.Empty();
		RedrawPathVisualization(PathPoints);
		return;
	}

	TArray<FPathVisPathPoint> NewPathPoints;
	
	if (auto MeshNavPath = Path->CastPath<FNavMeshPath>()) // navigation on a recast navmesh
	{
		if (MeshNavPath->IsStringPulled())
		{
			CreatePathVisPointsFromNavPathPoints(OwnerChar, MeshNavPath, NewPathPoints);
		} else
		{
			CreatePointsFromPathCorridor(OwnerChar, MeshNavPath, NewPathPoints);
		}
	} else // non-recast type of navigation data
	{
		UE_LOG(LogTemp, Error, TEXT("Path visualization for non-recast nav data is not implemented"));
	}

	// see if the actual point array has changed or not
	if (PathPoints.Num() != NewPathPoints.Num())
	{
		bUpdatedPathPoints = true;
		PathPoints = NewPathPoints;
	} else
	{
		for (int i = 0; i < PathPoints.Num(); i++)
		{
			if (NewPathPoints[i] != PathPoints[i])
			{
				bUpdatedPathPoints = true;
				PathPoints[i] = NewPathPoints[i];
			}
		}
	}
	
	if (bUpdatedPathPoints)
	{
		RedrawPathVisualization(PathPoints);
	}
}

void UPathVisComponent::DebugDescribePathEvent(ENavPathEvent::Type PathEvent)
{
	switch (PathEvent)
	{
	case ENavPathEvent::Type::Cleared:
		UE_LOG(LogTemp, Display, TEXT("Navigation path cleared!"));
		break;
	case ENavPathEvent::Type::Invalidated:
		UE_LOG(LogTemp, Display, TEXT("Navigation path invalidated!"));
		break;
	case ENavPathEvent::Type::Custom:
		UE_LOG(LogTemp, Display, TEXT("Custom path event"));
		break;
	case ENavPathEvent::Type::NewPath:
		UE_LOG(LogTemp, Display, TEXT("Path event: new path!"));
		break;
	case ENavPathEvent::Type::MetaPathUpdate:
		UE_LOG(LogTemp, Display, TEXT("Path event: meta path update!"));
		break;
	case ENavPathEvent::Type::RePathFailed:
		UE_LOG(LogTemp, Display, TEXT("Repath failed!"));
		break;
	case ENavPathEvent::Type::UpdatedDueToGoalMoved:
		UE_LOG(LogTemp, Display, TEXT("Path updated due to goal moved!"));
		break;
	case ENavPathEvent::Type::UpdatedDueToNavigationChanged:
		UE_LOG(LogTemp, Display, TEXT("Path updated due to navigation changed!"));
		break;
	}
}

void UPathVisComponent::DebugDescribePathFollowingResult(const FPathFollowingResult& Result)
{
	// TODO describe each flag in particular
	if (Result.IsInterrupted())
	{
		UE_LOG(LogTemp, Display, TEXT("PathFollowing interrupted!"));
	} else if (Result.IsFailure())
	{
		UE_LOG(LogTemp, Display, TEXT("PathFollowing failure!"));
	}  else if (Result.IsSuccess())
	{
		UE_LOG(LogTemp, Display, TEXT("PathFollowing success!"));
	}
}

void UPathVisComponent::RegisterOwnerPathPtrUpdates()
{
	// NOTE one could inherit from APathFollowingComponent and override the OnPathUpdated to notify about path changes
	// but this would bind us to the particular class, so this solution is intended to work with any APathFollowingComponent subclass
	
	// holding invalid pointer to a path while pf component is gone (not likely but still)
	if (!PFComponent && Path.IsValid())
	{
		Path.Reset();
		UpdatePathPoints(PathChanged);
		return;
	}

	// see if the path following component is pointing to a different path
	if (PFComponent->GetPath() != Path)
	{
		HandlePathChanged(PFComponent->GetPath());
	}
}

void UPathVisComponent::EnsurePathVisActor()
{
	if (!PathVisActor.IsValid())
	{
		PathVisActor = GetWorld()->SpawnActor(PathVisActorClass);
	}
}

UPathVisComponent::UPathVisComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	WaypointDistance = 100.f;
	KeypointRadius = 75.f;
}

void UPathVisComponent::RedrawPathVisualization_Implementation(const TArray<FPathVisPathPoint>& InPathPoints)
{
	EnsurePathVisActor();
	if (PathVisActor.IsValid() && PathVisActor->Implements<UUpdatablePathVisualization>())
	{
		IUpdatablePathVisualization::Execute_Update(&*PathVisActor, InPathPoints);
	}
}

void UPathVisComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RegisterOwnerPathPtrUpdates();
}

void UPathVisComponent::HandlePathChanged(FNavPathSharedPtr NewPath)
{
	Path = NewPath;

	UE_LOG(LogTemp, Display, TEXT("Received new path!"));
	
	FNavMeshPath* MeshNavPath = Path->CastPath<FNavMeshPath>();

	uint32 NumNavLinks = 0;
	for (const auto& PathPoint : MeshNavPath->GetPathPoints())
	{
		if (FNavMeshNodeFlags(PathPoint.Flags).IsNavLink())
		{
			NumNavLinks++;
		}
	}
	
	if (MeshNavPath)
	{
		UE_LOG(LogTemp, Display, TEXT("Path corridor length: %d"), MeshNavPath->PathCorridor.Num());
		UE_LOG(LogTemp, Display, TEXT("Is string-pulled: %hs"), MeshNavPath->IsStringPulled() ? "Yes" : "No");
		UE_LOG(LogTemp, Display, TEXT("Wants string pulling: %hs"), MeshNavPath->WantsStringPulling() ? "Yes" : "No");
		UE_LOG(LogTemp, Display, TEXT("Wants path corridor: %hs"), MeshNavPath->WantsPathCorridor() ? "Yes" : "No");
		UE_LOG(LogTemp, Display, TEXT("Number of path points: %d"), MeshNavPath->GetPathPoints().Num());
		UE_LOG(LogTemp, Display, TEXT("Number of nav links: %d"), NumNavLinks);
	}
	
	// subscribe to the path events
	NewPath->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UPathVisComponent::HandlePathEvent));
	UpdatePathPoints(PathChanged);
}

void UPathVisComponent::HandlePathEvent(FNavigationPath* InPath, ENavPathEvent::Type PathEvent)
{
	if (!InPath) { return; }
	DebugDescribePathEvent(PathEvent);
}

void UPathVisComponent::HandleMoveReqFinished(FAIRequestID Request, const FPathFollowingResult& Result)
{
	DebugDescribePathFollowingResult(Result);
	Path.Reset();
	UpdatePathPoints(DestinationReached);
}

void UPathVisComponent::HandleCharacterMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	// UE_LOG(LogTemp, Display, TEXT("Handling character move event for path visualization @delta seconds=%f"), DeltaSeconds);
	
	if (!(OwnerChar && PFComponent))
	{
		return;
	}

	// ignore movements not caused by actual path-following
	if (PFComponent->GetStatus() != EPathFollowingStatus::Moving)
	{
		return;
	}

	UpdatePathPoints(CharacterMoved);
}

// Called when the game starts
void UPathVisComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("%s"), *UUpdatablePathVisualization::StaticClass()->GetPathName());
	
	if (auto OwningController = Cast<AController>(GetOwner()))
	{
		OwnerChar = OwningController->GetCharacter();;

		if (!OwnerChar)
		{
			return;
		}
		
		// bind to the owning actor's movement updates!
		OwnerChar->OnCharacterMovementUpdated.AddDynamic(
			this,
			&UPathVisComponent::HandleCharacterMoved);

		// bind to path following component's events
		if (auto* PathFollowingComp = OwningController->FindComponentByClass<UPathFollowingComponent>())
		{
			this->PFComponent = PathFollowingComp;
			PathFollowingComp->OnRequestFinished.AddUObject(this, &UPathVisComponent::HandleMoveReqFinished);
		} else
		{
			UE_LOG(LogTemp, Error, TEXT("Path visualization component requires a path following component of type ABPathFollowingComponent on its owner"));
		}
	}
}

