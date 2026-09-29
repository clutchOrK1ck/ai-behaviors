// Fill out your copyright notice in the Description page of Project Settings.


#include "ABEnemyCharacter.h"

#include "Ballistics.h"


// Sets default values
AABEnemyCharacter::AABEnemyCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AABEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AABEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AABEnemyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AABEnemyCharacter::DoBeastlyJumpAt(const FVector& Target)
{
	if (!bCanPerformBeastlyJump)
	{
		return false;
	}

	TOptional<FPlanarBallisticTrajectory> Trajectory = FPlanarBallisticTrajectory::CreateChecked(
		this,
		this->GetActorLocation(),
		FVector(BeastlyJumpMaxVelocity, 0., 0.));

	if (!Trajectory.IsSet())
	{
		return false;
	}

	if (!Trajectory->Calibrate(Target, true))
	{
		return false;
	}

	LaunchCharacter(Trajectory->GetInitialVelocity(), true, true);
	return true;
}

