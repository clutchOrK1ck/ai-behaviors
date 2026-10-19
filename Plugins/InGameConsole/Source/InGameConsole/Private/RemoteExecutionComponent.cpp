// Fill out your copyright notice in the Description page of Project Settings.


#include "RemoteExecutionComponent.h"

#include "GameConsole.h"

bool URemoteCommandExecutor::FindOutputChannelsForCommand(const UConsoleCommand* ConsoleCommand,
                                                          bool bIsExecutingInForeground, ITextPipe*& OutStdOut, ITextPipe*& OutStdErr)
{
	OutStdOut = RemoteExecutionComponent->StdOut;
	OutStdErr = RemoteExecutionComponent->StdErr;

	return true;
}

void URemoteExecutionComponent::ReceiveCommandFeedbackOnClient_Implementation(const FString& Message,
	bool bIsStdErr)
{
	// push the feedback sent by the command from the server to the local game console stdout/stderr
	if (const UGameConsole* Console = GetWorld()->GetGameInstance()->GetSubsystem<UGameConsole>())
	{
		if (bIsStdErr)
		{
			Console->GetMutableStdErr()->Write(Message);
		} else
		{
			Console->GetMutableStdOut()->Write(Message);
		}
	}
}

bool URemoteCommandExecutor::AllowsMultipleProcesses() const
{
	return true; // TODO yes?
}

void URemoteCommandExecutor::Initialize(URemoteExecutionComponent* InRemoteExecutionComponent)
{
	this->RemoteExecutionComponent = InRemoteExecutionComponent;
}

URemoteExecutionComponent::URemoteExecutionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	StdOut = CreateDefaultSubobject<USimpleTextPipe>("StdOut");
	StdErr = CreateDefaultSubobject<USimpleTextPipe>("StdErr");

	RemoteCommandExecutor = CreateDefaultSubobject<URemoteCommandExecutor>("RemoteCommandExecutor");
	RemoteCommandExecutor->Initialize(this);

	// push command feedback back to the client
	StdOut->OnNewItem.AddLambda([this](const FString& Message)
	{
		this->ReceiveCommandFeedbackOnClient(Message, false);
	});

	StdErr->OnNewItem.AddLambda([this](const FString& Message)
	{
		this->ReceiveCommandFeedbackOnClient(Message, true);
	});
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

void URemoteExecutionComponent::ExecuteCommandOnServer_Implementation(TSubclassOf<UConsoleCommand> CommandClass,
	const FCommandArguments& Args)
{
	// server-side control for malicious execution attempts
	if (!IsPlayerAllowedToExecute(CommandClass.GetDefaultObject()))
	{
		return;
	}

	
}

void URemoteExecutionComponent::RequestServerExecutionAuthority_Implementation(EExecutionAuthorityType Type,
                                                                               const FString& Key)
{
	
}


bool URemoteExecutionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	
	return Cast<UWorld>(Outer)->GetNetMode() == NM_DedicatedServer || Cast<UWorld>(Outer)->GetNetMode() == NM_ListenServer;
}

void URemoteExecutionSubsystem::AddRemoteExecutionAuthorityRequest(const FExecutionAuthorityRequest& Request)
{
	PendingRequests.Enqueue(Request);
}

void URemoteExecutionSubsystem::GrantAuthority(const APlayerController* Player, EExecutionAuthorityType Type, FString Key)
{
	if (Player == nullptr || Player->FindComponentByClass<URemoteExecutionComponent>() == nullptr)
	{
		return;
	}

	FExecutionAuthority NewAuthority;

	NewAuthority.Type = Type;
	NewAuthority.Key = Key;

	// TODO double check that key is present if a particular type is supplied
	Player->FindComponentByClass<URemoteExecutionComponent>()->ExecutionAuthorityList.Add(NewAuthority);
}

bool URemoteExecutionSubsystem::GetOldestRequest(FExecutionAuthorityRequest& Request)
{
	if (FExecutionAuthorityRequest* RequestPtr = PendingRequests.Peek())
	{
		Request = *RequestPtr;
		return true;
	}

	return false;
}

void URemoteExecutionSubsystem::ProcessOldestRequest(EProcessRequestAction Action)
{
	if (PendingRequests.IsEmpty())
	{
		return;
	}

	FExecutionAuthorityRequest Request;
	PendingRequests.Dequeue(Request);
	
	if (Action == EProcessRequestAction::Deny)
	{
		return;
	}

	GrantAuthority(Request.Requester, Request.Type, Request.Key);
}

void URemoteExecutionSubsystem::PurgeAllRequests()
{
	PendingRequests.Empty();
}


