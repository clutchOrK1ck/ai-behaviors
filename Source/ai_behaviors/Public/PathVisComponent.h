// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ABPathFollowingComponent.h"
#include "Components/ActorComponent.h"
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

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class AI_BEHAVIORS_API UPathVisComponent : public UActorComponent
{
	GENERATED_BODY()

	FNavPathSharedPtr Path;
	TArray<FPathVisPathPoint> PathPoints; // at the moment, this is used only as cache for points built during the last update
	UABPathFollowingComponent* PFComponent; // the owner's path following component
	ACharacter* OwnerChar;
	
	void UpdatePathPoints(EPathPointsUpdateReason Reason);
	void DebugDescribePathEvent(ENavPathEvent::Type PathEvent);
	void DebugDescribePathFollowingResult(const FPathFollowingResult& Result);
	
public:
	// Sets default values for this component's properties
	UPathVisComponent();

	// blueprints implement this to draw the actual visualization
	UFUNCTION(BlueprintImplementableEvent, meta=(ForceAsFunction))
	void RedrawPathVisualization(const TArray<FPathVisPathPoint>& InPathPoints);

protected:
	virtual void HandlePathChanged(FNavPathSharedPtr NewPath);
	virtual void HandlePathEvent(FNavigationPath* InPath, ENavPathEvent::Type PathEvent);
	virtual void HandleMoveReqFinished(FAIRequestID, const FPathFollowingResult& Result);
	UFUNCTION()
	virtual void HandleCharacterMoved(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	
	// Called when the game starts
	virtual void BeginPlay() override;
};
