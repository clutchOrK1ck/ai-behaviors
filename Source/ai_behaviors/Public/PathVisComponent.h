// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ABPathFollowingComponent.h"
#include "Components/ActorComponent.h"
#include "NavMesh/NavMeshPath.h"
#include "PathVisComponent.generated.h"


enum EPathPointsUpdateReason
{
	DestinationReached,
	PathChanged,
	CharacterMoved
};

UENUM(BlueprintType)
enum EPathVisualizationPointType
{
	Waypoint,
	Navlink,
	Destination
};

/**
 * a nav path point, containing additional meta-data for path visualization
 */
USTRUCT(BlueprintType)
struct FPathVisPathPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FVector Location;

	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EPathVisualizationPointType> Type;

	bool operator==(const FPathVisPathPoint& Other) const = default;
};

UINTERFACE(BlueprintType)
class AI_BEHAVIORS_API UUpdatablePathVisualization : public UInterface
{
	GENERATED_BODY()
};

class AI_BEHAVIORS_API IUpdatablePathVisualization
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void Update(const TArray<FPathVisPathPoint>& PathPoints);
};

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class AI_BEHAVIORS_API UPathVisComponent : public UActorComponent
{
	GENERATED_BODY()

	FNavPathSharedPtr Path;
	TArray<FPathVisPathPoint> PathPoints; // at the moment, this is used only as cache for points built during the last update
	UPathFollowingComponent* PFComponent; // the owner's path following component
	ACharacter* OwnerChar;

	// the path visualization actor, managed by this component
	TWeakObjectPtr<AActor> PathVisActor;
	
	void UpdatePathPoints(EPathPointsUpdateReason Reason);
	void DebugDescribePathEvent(ENavPathEvent::Type PathEvent);
	void DebugDescribePathFollowingResult(const FPathFollowingResult& Result);

	void RegisterOwnerPathPtrUpdates();

	// ensure path vis actor exists and is valid
	void EnsurePathVisActor();

	// create path visualization points from a string-pulled path
	void CreatePathVisPointsFromNavPathPoints(const ACharacter* MovingChar, FNavMeshPath* InPath, TArray<FPathVisPathPoint>& OutPoints);

	// create path visualization points from the path corridor
	void CreatePointsFromPathCorridor(const ACharacter* MovingChar, FNavMeshPath* InPath, TArray<FPathVisPathPoint>& OutPoints);
	
public:
	// Sets default values for this component's properties
	UPathVisComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MustImplement = "/Script/ai_behaviors.UpdatablePathVisualization"))
	TSubclassOf<AActor> PathVisActorClass;
	
	// blueprints implement this to draw the actual visualization
	UFUNCTION(BlueprintNativeEvent, meta=(ForceAsFunction))
	void RedrawPathVisualization(const TArray<FPathVisPathPoint>& InPathPoints);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	
protected:
	virtual void HandlePathChanged(FNavPathSharedPtr NewPath);
	virtual void HandlePathEvent(FNavigationPath* InPath, ENavPathEvent::Type PathEvent);
	virtual void HandleMoveReqFinished(FAIRequestID, const FPathFollowingResult& Result);
	UFUNCTION()
	virtual void HandleCharacterMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	
	// Called when the game starts
	virtual void BeginPlay() override;
};
