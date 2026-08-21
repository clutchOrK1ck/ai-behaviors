// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/AIModule/Classes/Navigation/PathFollowingComponent.h"
#include "ABPathFollowingComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AI_BEHAVIORS_API UABPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()
	
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPathUpdated, FNavPathSharedPtr NewPath);

	/**
	 * a delegate that notifies about path updates (received new path etc)
	 */
	FOnPathUpdated OnPathUpdatedEvent;
	
	// Sets default values for this component's properties
	UABPathFollowingComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnPathUpdated() override;
};
