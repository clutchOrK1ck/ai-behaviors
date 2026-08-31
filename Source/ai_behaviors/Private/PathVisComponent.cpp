// Fill out your copyright notice in the Description page of Project Settings.


#include "PathVisComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavMesh/NavMeshPath.h"


struct FPolyLine
{
	TArray<FVector> Points;

	/**
	 * distributes points on this poly line
	 * @param Distance distance between points
	 * @param OutPoints 
	 */
	void DistributePoints(float Distance, TArray<FVector>& OutPoints)
	{
		// TODO more efficient implementation?
		for (int i = 0; i < GetLength() / Distance; i++)
		{
			OutPoints.Add(GetLocationAtDistance(i * Distance));
		}
	}
	
	FVector GetLocationAtDistance(float Distance)
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
	
	float GetLength()
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

void UPathVisComponent::CreatePathVisPointsFromNavPathPoints(const ACharacter* MovingChar,
	FNavMeshPath* InPath, TArray<FPathVisPathPoint>& OutPoints)
{
	// create a spline from the nav path to distribute points on it
	FPolyLine PathPolyLine;
	
	// create the polyline using path's points
	for (int PointIdx = InPath->GetPathPoints().Num() - 1; PointIdx >= 0; PointIdx--)
	{
		FVector NavPathLocation = *Path->GetPathPointLocation(PointIdx);
		PathPolyLine.Points.Add(NavPathLocation);
	}

	// calculate the distance remaining for the character to cover on this path
	auto CurrentPathPointTargetId = PFComponent->GetCurrentPathElement();
	auto CharacterLocation = OwnerChar->GetActorLocation();
	float RemainingDistance = Path->GetLengthFromPosition(MovingChar->GetActorLocation(), CurrentPathPointTargetId);

	// distribute points on the path, but not to cover more than the remaining distance
	for (int i = 0; i < RemainingDistance / 100.f; i++)
	{
		// make everything a waypoint for the time being, can always adjust if there's a need
		FPathVisPathPoint Pt {
			PathPolyLine.GetLocationAtDistance(i * 100.f),
			Waypoint
		};
		OutPoints.Add(Pt);
	}
}

void UPathVisComponent::CreatePointsFromPathCorridor(const ACharacter* MovingChar,
	FNavMeshPath* InPath,
	TArray<FPathVisPathPoint>& OutPoints)
{
	FPolyLine PathPolyLine;

	PathPolyLine.Points.Add(InPath->GetDestinationLocation());
	
	for (int i = InPath->GetPathCorridorEdges().Num() - 1; i >= 0; i--)
	{
		// TODO do not include edges already crossed
		
		auto Edge = InPath->GetPathCorridorEdges()[i];
		PathPolyLine.Points.Add(Edge.Left + (Edge.Right - Edge.Left) / 2.);
	}
	
	PathPolyLine.Points.Add(MovingChar->GetCharacterMovement()->GetFeetLocation());

	TArray<FVector> Pts;
	PathPolyLine.DistributePoints(100.f, Pts);

	for (auto Pt : Pts)
	{
		FPathVisPathPoint PathVisPt {
			Pt, Waypoint
		};
		OutPoints.Add(PathVisPt);
	}
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
	if (MeshNavPath)
	{
		UE_LOG(LogTemp, Display, TEXT("Path corridor length: %d"), MeshNavPath->PathCorridor.Num());
		UE_LOG(LogTemp, Display, TEXT("Is string-pulled: %hs"), MeshNavPath->IsStringPulled() ? "Yes" : "No");
		UE_LOG(LogTemp, Display, TEXT("Wants string pulling: %hs"), MeshNavPath->WantsStringPulling() ? "Yes" : "No");
		UE_LOG(LogTemp, Display, TEXT("Wants path corridor: %hs"), MeshNavPath->WantsPathCorridor() ? "Yes" : "No");
		UE_LOG(LogTemp, Display, TEXT("Number of path points: %d"), MeshNavPath->GetPathPoints().Num());
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

