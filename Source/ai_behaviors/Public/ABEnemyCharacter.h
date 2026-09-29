// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ABEnemyCharacter.generated.h"

UCLASS()
class AI_BEHAVIORS_API AABEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AABEnemyCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/**
	 * we would want an ability system or such for this, but with just a couple of enemies in the demo it's fine
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCanPerformBeastlyJump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BeastlyJumpMaxVelocity;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	bool CanPerformBeastlyJump() const
	{
		return bCanPerformBeastlyJump;
	}

	float GetBeastlyJumpMaxVelocity() const {
		return BeastlyJumpMaxVelocity;
	}

	/**
	 * performs a beastly jump at a target
	 * @param Target target to jump at
	 * @return if the jump was actually performed (it can fail to if the character cannot beastly-jump or target is unreachable)
	 */
	bool DoBeastlyJumpAt(const FVector& Target);
};
