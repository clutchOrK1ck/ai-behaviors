// Fill out your copyright notice in the Description page of Project Settings.


#include "ABAIController.h"

#include "ABEnemyCharacter.h"
#include "ABPathFollowingComponent.h"
#include "Ballistics.h"
#include "GameFramework/Character.h"


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

bool AABAIController::BeastlyJump(const FVector& Target)
{
	if (const auto ABEnemyChar = Cast<AABEnemyCharacter>(this->GetCharacter()))
	{
		return ABEnemyChar->DoBeastlyJumpAt(Target);
	}
	return false;
}

