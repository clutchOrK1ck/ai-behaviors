// Fill out your copyright notice in the Description page of Project Settings.


#include "ABAIController.h"

#include "ABPathFollowingComponent.h"


// Sets default values
AABAIController::AABAIController(const FObjectInitializer& ObjectInitializer) : Super(
	ObjectInitializer.SetDefaultSubobjectClass<UABPathFollowingComponent>(
		TEXT("PathFollowingComponent"))
)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AABAIController::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AABAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

