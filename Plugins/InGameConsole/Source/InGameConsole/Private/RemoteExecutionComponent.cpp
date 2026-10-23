// Fill out your copyright notice in the Description page of Project Settings.


#include "RemoteExecutionComponent.h"

#include "GameConsole.h"
#include "Net/UnrealNetwork.h"

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

URemoteExecutionComponent::URemoteExecutionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	this->SetIsReplicatedByDefault(true);

	StdOut = CreateDefaultSubobject<USimpleTextPipe>("StdOut");
	StdErr = CreateDefaultSubobject<USimpleTextPipe>("StdErr");

	RemoteCommandExecutor = CreateDefaultSubobject<UCommandExecutor>("RemoteCommandExecutor");
	RemoteCommandExecutor->bAllowsParallelExecution = false;
	RemoteCommandExecutor->bCanExecuteLatentCommands = false;

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

bool URemoteExecutionComponent::IsPlayerAllowedToExecute(const UConsoleCommandBase* Command) const
{
	// no execution of latent commands
	if (Command->IsA<ULatentConsoleCommand>())
	{
		return false;
	}
	
	for (const FExecutionAuthority& Auth : ExecutionAuthorityList)
	{
		switch(Auth.Type)
		{
		case EExecutionAuthorityType::All:
			return true;
		case EExecutionAuthorityType::AllInCategory:
			if (Command->Category.Equals(Auth.Key)) // TODO categories can be empty
			{
				return true;
			}
			break;
		case EExecutionAuthorityType::SingleCommand:
			if (Command->CommandName.ToString().Equals(Auth.Key))
			{
				return true;
			}
			break;
		default:
			return false;
		}
	}

	return false;
}

APlayerController* URemoteExecutionComponent::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

void URemoteExecutionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(URemoteExecutionComponent,
		ExecutionAuthorityList,
		COND_OwnerOnly);
}

void URemoteExecutionComponent::ExecuteCommandOnServer_Implementation(const FString& Command)
{
	// parse command on server using server's console
	UConsoleCommandBase* CommandTemplate;
	FCommandArguments Args;
	FString ParsingError;

	if (!GetWorld()->GetGameInstance()->GetSubsystem<UGameConsole>()->ParseCommand(Command, CommandTemplate, Args, ParsingError))
	{
		StdErr->Write(ParsingError); // this will propagate over network if the altered client bypassed parsing
		return;
	}

	if (!CommandTemplate->bIsServerCommand)
	{
		StdErr->Write("Remote execution of local commands is not allowed");
		return;
	}
	
	// server-side control for malicious execution attempts
	if (!IsPlayerAllowedToExecute(CommandTemplate))
	{
		StdErr->Write(FString::Format(TEXT("This client is not allowed to execute '{0}'"), {CommandTemplate->CommandName.ToString()}));
		return;
	}
	
	RemoteCommandExecutor->Execute(GetOwningController(), CommandTemplate->GetClass(), Args, StdOut, StdErr);
}

void URemoteExecutionComponent::RequestServerExecutionAuthority_Implementation(EExecutionAuthorityType Type,
                                                                               const FString& Key)
{
	// create new authority request in server's remote execution subsystem
	FExecutionAuthorityRequest NewRequest;

	NewRequest.Requester = GetOwningController();
	NewRequest.Type = Type;
	NewRequest.Key = Key;

	if (auto RemoteExecutionSubsystem = GetWorld()->GetSubsystem<URemoteExecutionSubsystem>())
	{
		RemoteExecutionSubsystem->AddRemoteExecutionAuthorityRequest(NewRequest);
	} else
	{
		UE_LOG(LogTemp, Error, TEXT("Remote execution subsystem not found"));
	}
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

	if (!Request.Requester.IsValid())
	{
		return;
	}
	
	if (Action == EProcessRequestAction::Deny)
	{
		return;
	}

	GrantAuthority(Request.Requester.Get(), Request.Type, Request.Key);
}

void URemoteExecutionSubsystem::PurgeAllRequests()
{
	PendingRequests.Empty();
}


