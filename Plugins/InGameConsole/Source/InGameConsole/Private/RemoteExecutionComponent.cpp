// Fill out your copyright notice in the Description page of Project Settings.


#include "RemoteExecutionComponent.h"

URemoteExecutionComponent::URemoteExecutionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// TODO replication
}

bool URemoteExecutionComponent::IsPlayerAllowedToExecute(const UConsoleCommand* Command) const
{
	for (const FExecutionAuthority& Auth : ExecutionAuthorityList)
	{
		switch(Auth.Type)
		{
		case EExecutionAuthorityType::All:
			return true;
		case EExecutionAuthorityType::AllInCategory:
			return Command->Category.Equals(Auth.Key); // TODO categories can be empty
		case EExecutionAuthorityType::SingleCommand:
			return Command->CommandName.ToString().Equals(Auth.Key);
		default:
			return false;
		}
	}

	return false;
}

void URemoteExecutionComponent::RequestServerExecutionAuthority_Implementation(EExecutionAuthorityType Type,
	const FString& Key)
{
	
}

void URemoteExecutionComponent::ExecuteServerCommand_Implementation(const FString& Command)
{
	// TODO send the command to the game console + this happens on the server...
}


bool URemoteExecutionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	
	return Cast<UWorld>(Outer)->GetNetMode() == NM_DedicatedServer || Cast<UWorld>(Outer)->GetNetMode() == NM_ListenServer;
}

void URemoteExecutionSubsystem::ProcessOldestRequest(EProcessRequestAction Action)
{
	if (PendingRequests.IsEmpty())
	{
		return;
	}

	FExecutionAuthorityRequest Request;
	PendingRequests.Dequeue(Request);

	if (Request.Requester == nullptr || Request.Requester->FindComponentByClass<URemoteExecutionComponent>() == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Requesting player controller is nullptr or has no valid remote execution component"));
		return;
	}
	
	if (Action == EProcessRequestAction::Deny)
	{
		return;
	}

	FExecutionAuthority NewAuthority;

	NewAuthority.Type = Request.Type;
	NewAuthority.Key = Request.Key;

	Request.Requester->FindComponentByClass<URemoteExecutionComponent>()->ExecutionAuthorityList.Add(NewAuthority);
}

void URemoteExecutionSubsystem::PurgeAllRequests()
{
	PendingRequests.Empty();
}


