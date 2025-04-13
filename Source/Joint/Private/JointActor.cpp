//Copyright 2022~2024 DevGrain. All Rights Reserved.


#include "JointActor.h"

#include "JointFragment.h"
#include "JointManager.h"
#include "JointNodeBase.h"
#include "Joint.h"
#include "JointSubsystem.h"

#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include "Misc/EngineVersionComparison.h"

//Debug related macro.
#define DEBUG_ShowNodeEvent 0 && WITH_EDITOR
#define DEBUG_ShowNodeEvent_BeginPlay 1 && DEBUG_ShowNodeEvent
#define DEBUG_ShowNodeEvent_EndPlay 1 && DEBUG_ShowNodeEvent
#define DEBUG_ShowNodeEvent_Pending 1 && DEBUG_ShowNodeEvent

#define DEBUG_ShowJointEvent 0 && WITH_EDITOR
#define DEBUG_ShowJointEvent_SetJoint 1 && DEBUG_ShowJointEvent
#define DEBUG_ShowJointEvent_StartJoint 1 && DEBUG_ShowJointEvent
#define DEBUG_ShowJointEvent_EndJoint 1 && DEBUG_ShowJointEvent
#define DEBUG_ShowJointEvent_DiscardJoint 0 && DEBUG_ShowJointEvent
#define DEBUG_ShowJointEvent_DestroyJoint 0 && DEBUG_ShowJointEvent
#define DEBUG_ShowJointEvent_PlayNextNode 1 && DEBUG_ShowJointEvent
#define DEBUG_ShowJointEvent_SetPlayingNode 1 && DEBUG_ShowJointEvent


#define DEBUG_ShowReplication 0 && WITH_EDITOR

#define USE_NEW_REPLICATION !UE_VERSION_OLDER_THAN(5, 1, 0) && true


// Sets default values
AJointActor::AJointActor()
{
	//Joint Actor can not tick.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

#if USE_NEW_REPLICATION

	bReplicateUsingRegisteredSubObjectList = true;

#endif

	bAlwaysRelevant = true;

	ImplementAbilitySystemComponent();

	JointGuid = FGuid::NewGuid();
}

UAbilitySystemComponent* AJointActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AJointActor::ImplementAbilitySystemComponent()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
}

void AJointActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndJoint();
	
	Super::EndPlay(EndPlayReason);
}

UJointNodeBase* AJointActor::GetPlayingJointNode()
{
	return PlayingJointNode;
}

void AJointActor::OnRep_CachedAllNodesForNetworking(
	const TArray<UJointNodeBase*>& PreviousCachedAllNodesForNetworking)
{
#if USE_NEW_REPLICATION
	
	if (IsUsingRegisteredSubObjectList())
	{
		// Find the attributes that got removed
		for (UJointNodeBase* PreviousNode : PreviousCachedAllNodesForNetworking)
		{
			if (PreviousNode)
			{
				const bool bWasRemoved = CachedAllNodesForNetworking.Find(PreviousNode) == INDEX_NONE;
				
				if (bWasRemoved)
				{
					RemoveReplicatedSubObject(PreviousNode);
				}
			}
		}

		// Find the attributes that got added
		for (UJointNodeBase* NewPreviousNode : PreviousCachedAllNodesForNetworking)
		{
			if (IsValid(NewPreviousNode))
			{
				const bool bIsAdded = PreviousCachedAllNodesForNetworking.Find(NewPreviousNode) == INDEX_NONE;
				
				if (bIsAdded)
				{
					AddReplicatedSubObject(NewPreviousNode);
				}
			}
		}
	}
	
#endif

	/* Clean Up code for removed nodes.
	for (UJointNodeBase* PreviousNode : PreviousCachedAllNodesForNetworking)
	{
		if (PreviousNode && CachedAllNodesForNetworking.Find(PreviousNode) == INDEX_NONE)
		{

		}
	}
	*/
}

void AJointActor::RequestSetJointManager_Implementation(UJointManager* NewJointManager)
{
	if (NewJointManager != nullptr)
	{
		UJointManager* DuplicatedJointManager = DuplicateObject<UJointManager>(NewJointManager, this);

#if WITH_EDITOR
		//For debugging purpose.

		OriginalJointManager = NewJointManager;
#endif

		JointManager = DuplicatedJointManager;
		//SetJointManager(DuplicatedJointManager);

#if DEBUG_ShowJointEvent_SetJoint
		
		if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
		{
			if (GEngine)
				GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(
													 TEXT("%s, %s, %s: Joint Setted, Joint Manager : %s"),
													 UKismetSystemLibrary::IsStandalone(this)
														 ? *FString("Standalone")
														 : UKismetSystemLibrary::IsDedicatedServer(this)
														 ? *FString("Dedicated Server")
														 : UKismetSystemLibrary::IsServer(this)
														 ? *FString("Listen Server (Client_Host)")
														 : *FString("Client"),
													 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
													 *this->GetName(),
													 *JointManager->GetName()));
		}
		
#endif
	}
}

UJointManager* AJointActor::GetJointManager()
{
	return JointManager;
}

void AJointActor::OnNotifiedCurrentNodePending(UJointNodeBase* Node)
{
#if DEBUG_ShowNodeEvent_Pending
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(
			                                 TEXT("%s, %s, %s: OnNotifiedCurrentNodePending, Node : %s "),
			                                 UKismetSystemLibrary::IsStandalone(this)
				                                 ? *FString("Standalone")
				                                 : UKismetSystemLibrary::IsDedicatedServer(this)
				                                 ? *FString("Dedicated Server")
				                                 : UKismetSystemLibrary::IsServer(this)
				                                 ? *FString("Listen Server (Client_Host)")
				                                 : *FString("Client"),
			                                 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
			                                 *this->GetName(),
			                                 Node ? *Node->GetName() : *FString()));
#endif

	//On Client side, stop the current playing Joint.
	RequestNodeEndPlay(PlayingJointNode);

	//Let it move only when this client has the authority over the actor.
	//This little shit does work like freaking flip flop every time when try to do it from scratch.
	//This is because setting off the Joint manager and actor properly takes a deeper understanding of replication and networking.
	//Maybe I have to mention about it on the tutorial or other stuffs.
}

void AJointActor::OnNotifiedCurrentNodeEnded(UJointNodeBase* Node)
{
	if (HasAuthority())
	{
		PlayNextNode();
	}
}

void AJointActor::ReleaseEventsFromPlayingJointNode_Implementation()
{
	//Remove old delegate
	if (PlayingJointNode == nullptr) return;

	PlayingJointNode->OnJointNodeMarkedAsPendingDelegate.RemoveAll(this);

	PlayingJointNode->OnJointNodeEndDelegate.RemoveAll(this);
}

void AJointActor::BindEventsOnPlayingJointNode_Implementation()
{
	if (PlayingJointNode == nullptr) return;

	//Bind delegate
	PlayingJointNode->OnJointNodeMarkedAsPendingDelegate.RemoveAll(this);

	PlayingJointNode->OnJointNodeEndDelegate.RemoveAll(this);


	PlayingJointNode->OnJointNodeMarkedAsPendingDelegate.AddDynamic(
		this, &AJointActor::OnNotifiedCurrentNodePending);

	PlayingJointNode->OnJointNodeEndDelegate.AddDynamic(
		this, &AJointActor::OnNotifiedCurrentNodeEnded);
}

void AJointActor::SetPlayingJointNode_Implementation(UJointNodeBase* NewPlayingJointNode)
{
	PlayingJointNode = NewPlayingJointNode;

	//Reload the node's activity related flags.
	if (PlayingJointNode) PlayingJointNode->ReloadNode();

#if DEBUG_ShowJointEvent_SetPlayingNode
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(
			                                 TEXT("%s, %s, %s: Joint Set PlayingNode, Joint Manager : %s"),
			                                 UKismetSystemLibrary::IsStandalone(this)
				                                 ? *FString("Standalone")
				                                 : UKismetSystemLibrary::IsDedicatedServer(this)
				                                 ? *FString("Dedicated Server")
				                                 : UKismetSystemLibrary::IsServer(this)
				                                 ? *FString("Listen Server (Client_Host)")
				                                 : *FString("Client"),
			                                 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
			                                 *this->GetName(),
			                                 PlayingJointNode ? *PlayingJointNode->GetName() : *FString("Null")));


	UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: Joint Set PlayingNode, Joint Manager : %s"),
		       UKismetSystemLibrary::IsStandalone(this)
		       ? *FString("Standalone")
		       : UKismetSystemLibrary::IsDedicatedServer(this)
		       ? *FString("Dedicated Server")
		       : UKismetSystemLibrary::IsServer(this)
		       ? *FString("Listen Server (Client_Host)")
		       : *FString("Client"),
		       *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
		       *this->GetName(),
		       *JointManager->GetName()));

#endif
}

void AJointActor::BeginPlayPlayingJointNode_Implementation()
{
	if (PlayingJointNode)
	{
		if (OnJointBaseNodePlayedDelegate.IsBound())
		{
			OnJointBaseNodePlayedDelegate.Broadcast(this, PlayingJointNode);
		}
		RequestNodeBeginPlay(PlayingJointNode);
	}
}

void AJointActor::EndPlayPlayingJointNode_Implementation()
{
	if (PlayingJointNode)
	{
		RequestNodeEndPlay(PlayingJointNode);
	}
}

UJointNodeBase* AJointActor::PickUpNewNodeFrom(const TArray<UJointNodeBase*>& TestTargetNodes)
{
	for (UJointNodeBase* TargetNode : TestTargetNodes)
	{
		if (!TargetNode) continue;

		return TargetNode;
	}

	return nullptr;
}

bool AJointActor::IsJointStarted()
{
	return bIsJointStarted;
}

bool AJointActor::IsJointEnded()
{
	return bIsJointEnded;
}

void AJointActor::BeginManagerFragments()
{
	if (!JointManager) return;

	for (UJointNodeBase* ManagerFragment : JointManager->ManagerFragments)
	{
		if (ManagerFragment == nullptr) continue;
		
		ManagerFragment->RequestNodeBeginPlay(this);
	}
}

void AJointActor::EndManagerFragments()
{
	if (!JointManager) return;

	for (UJointNodeBase* ManagerFragment : JointManager->ManagerFragments)
	{
		if (ManagerFragment == nullptr) continue;

		ManagerFragment->RequestNodeEndPlay();
	}
}


void AJointActor::StartJoint_Implementation()
{
	if (IsJointStarted()) return;

	if (JointManager == nullptr) return;

#if DEBUG_ShowJointEvent_StartJoint

	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(
			                                 TEXT("%s, %s, %s: JointStarted, Joint Manager : %s"),
			                                 UKismetSystemLibrary::IsStandalone(this)
				                                 ? *FString("Standalone")
				                                 : UKismetSystemLibrary::IsDedicatedServer(this)
				                                 ? *FString("Dedicated Server")
				                                 : UKismetSystemLibrary::IsServer(this)
				                                 ? *FString("Listen Server (Client_Host)")
				                                 : *FString("Client"),
			                                 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
			                                 *this->GetName(),
			                                 *JointManager->GetName()));

	UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: JointStarted, Joint Manager : %s"),
		       UKismetSystemLibrary::IsStandalone(this)
		       ? *FString("Standalone")
		       : UKismetSystemLibrary::IsDedicatedServer(this)
		       ? *FString("Dedicated Server")
		       : UKismetSystemLibrary::IsServer(this)
		       ? *FString("Listen Server (Client_Host)")
		       : *FString("Client"),
		       *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
		       *this->GetName(),
		       *JointManager->GetName()));

#endif

#if WITH_EDITOR

	if (FJointModule* Module = &FModuleManager::GetModuleChecked<FJointModule>("Joint"); Module != nullptr && Module->OnJointDebuggerMentionJointBeginPlay.IsBound())
	{
		Module->OnJointDebuggerMentionJointBeginPlay.ExecuteIfBound(this, this->JointGuid);
	}

#endif
	
	CacheNodesForNetworking();
	
	//Multicast the actual action on the Joint start event.
	ProcessStartJoint();

	//Pick up the new node to play.
	SetPlayingJointNode(PickUpNewNodeFrom(JointManager->StartNodes));
	
	//Check we have actually playing Joint node. If not, just end it here.
	if (PlayingJointNode == nullptr)
	{
		EndJoint();
	}else
	{
		BindEventsOnPlayingJointNode();

		BeginPlayPlayingJointNode();	
	}
	
}

void AJointActor::ProcessStartJoint_Implementation()
{
	MarkAsStarted();

	NotifyStartJoint();

	if (OnJointStartedDelegate.IsBound())
	{
		OnJointStartedDelegate.Broadcast(this);
	}

	BeginManagerFragments();
}


void AJointActor::EndJoint_Implementation()
{
	if (IsJointEnded()) return;

	if (JointManager == nullptr) return;

#if DEBUG_ShowJointEvent_EndJoint

	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(TEXT("%s, %s, %s: JointEnded"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
												 *this->GetName()));


		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: JointEnded"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
				   *this->GetName()));
	}
#endif

	ReleaseEventsFromPlayingJointNode();

	EndPlayPlayingJointNode();
	
	DiscardJoint();
	
	//Multicast the actual action on the Joint end event.
	ProcessEndJoint();

#if WITH_EDITOR

	if (FJointModule* Module = &FModuleManager::GetModuleChecked<FJointModule>("Joint"); Module != nullptr && Module->OnJointDebuggerMentionJointEndPlay.IsBound())
	{
		Module->OnJointDebuggerMentionJointEndPlay.ExecuteIfBound(this, this->JointGuid);
	}

#endif
	
	//Clear it if it's not being destroyed.
	if(!IsActorBeingDestroyed()) DestroyJoint();
}

void AJointActor::ProcessEndJoint_Implementation()
{
	EndManagerFragments();

	ForceEndAllKnownActiveNodes();

	MarkAsEnded();

	NotifyEndJoint();

	if (OnJointEndedDelegate.IsBound())
	{
		OnJointEndedDelegate.Broadcast(this);
	}
}

void AJointActor::ForceEndAllKnownActiveNodes()
{
	TArray<UJointNodeBase*> CopiedKnownActiveNodes = KnownActiveNodes;

	for (int i = CopiedKnownActiveNodes.Num() - 1; i > INDEX_NONE; --i)
	{
		if (UJointNodeBase* NodeBase = CopiedKnownActiveNodes[i]; NodeBase && NodeBase->IsValidLowLevel())
			RequestNodeEndPlay(NodeBase);
	}

	KnownActiveNodes.Empty();
}

void AJointActor::DiscardJoint_Implementation()
{
#if DEBUG_ShowJointEvent_DiscardJoint

	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(TEXT("%s, %s, %s: DiscardJoint"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 (GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0)) ? *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName() : *FString(),
												 *this->GetName()));


		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: DiscardJoint"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   (GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0)) ? *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName() : *FString(),
				   *this->GetName()));
	}
#endif

	ReleaseEventsFromPlayingJointNode();

	ForceEndAllKnownActiveNodes();
}

void AJointActor::DestroyJoint_Implementation()
{
#if DEBUG_ShowJointEvent_DestroyJoint

	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(TEXT("%s, %s, %s: DestroyJoint"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
												 *this->GetName()));


		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: DestroyJoint"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
				   *this->GetName()));
	}
	
#endif
	
	if (IsValidLowLevel() && GetWorld())
	{
		//Destroy on the next tick for additional clean up.
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AJointActor::DestroyInstance);
	}
}

void AJointActor::DestroyInstance()
{
	Destroy();
}


void AJointActor::MarkAsStarted()
{
	bIsJointStarted = true;
}

void AJointActor::MarkAsEnded()
{
	bIsJointEnded = true;
}

void AJointActor::NotifyStartJoint()
{
	if (!IsJointStarted())
		if (UJointSubsystem* SubSystem = UJointSubsystem::Get(this)) SubSystem->OnJointStarted(this);
}

void AJointActor::NotifyEndJoint()
{
	if (!IsJointEnded())
		if (UJointSubsystem* SubSystem = UJointSubsystem::Get(this)) SubSystem->OnJointEnded(this);
}


void AJointActor::PlayNextNode_Implementation()
{
	if (JointManager == nullptr) return;

#if DEBUG_ShowJointEvent_PlayNextNode

	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Emerald, FString::Printf(TEXT("%s, %s, %s: PlayNextNode"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
												 *this->GetName()));


		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: PlayNextNode"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
				   *this->GetName()));
	}
	
#endif

	EndPlayPlayingJointNode();

	ReleaseEventsFromPlayingJointNode();

	//Select new node from the last node.
	if (PlayingJointNode) SetPlayingJointNode(PickUpNewNodeFrom(PlayingJointNode->SelectNextNodes(this)));

	if (PlayingJointNode == nullptr)
	{
		EndJoint();
	}else
	{
		BindEventsOnPlayingJointNode();

		BeginPlayPlayingJointNode();	
	}
}

void AJointActor::ProcessPlayNextNode_Implementation()
{
}

void AJointActor::RequestNodeBeginPlay(UJointNodeBase* InNode)
{
	if (!InNode) return;

	if (InNode->IsNodeBegunPlay()) return;


#if DEBUG_ShowNodeEvent_BeginPlay

	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(
												 TEXT("%s, %s, %s: RequestNodeBeginPlay, Node : %s"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
												 *this->GetName(),
												 *InNode->GetName()));

		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: RequestNodeBeginPlay, Node : %s"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
				   *this->GetName(),
				   *InNode->GetName()));
	}

#endif
	
	if (InNode->GetParentNode())
	{
		//Check if the node has parent node and parent node is active. otherwise, don't start it.

		if (!InNode->GetParentNode()->IsNodeBegunPlay() || InNode->GetParentNode()->IsNodeEndedPlay()) return;
	}
	else
	{
		//If it is a base node, then check whether we are actually playing it, while this node is not a manager fragment.
		if (this->PlayingJointNode != InNode && !GetJointManager()->ManagerFragments.Contains(InNode)) return;
	}

	InNode->SetHostingJointInstance(this);

#if WITH_EDITOR

	if (CheckDebuggerCanProceed(InNode)) return;

#endif

#if WITH_EDITOR

	if (FJointModule* Module = &FModuleManager::GetModuleChecked<FJointModule>("Joint"); Module != nullptr && Module->
		OnJointDebuggerMentionNodeBeginPlay.IsBound())
	{
		Module->OnJointDebuggerMentionNodeBeginPlay.ExecuteIfBound(this, InNode);
	}

#endif

	KnownActiveNodes.Add(InNode);

	InNode->NodeBeginPlay();
}

void AJointActor::RequestMarkNodeAsPending(UJointNodeBase* InNode)
{
	if (!InNode) return;

	if (InNode->IsNodePending()) return;

#if WITH_EDITOR


	if (FJointModule* Module = &FModuleManager::GetModuleChecked<FJointModule>("Joint"); Module != nullptr && Module->
		OnJointDebuggerMentionNodePending.IsBound())
	{
		Module->OnJointDebuggerMentionNodePending.ExecuteIfBound(this, InNode);
	}

#endif

#if DEBUG_ShowNodeEvent_Pending

	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(
												 TEXT("%s, %s, %s: RequestMarkNodeAsPending, Node : %s"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
												 *this->GetName(), *InNode->GetName()));

		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: RequestMarkNodeAsPending, Node : %s"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
				   *this->GetName(),
				   *InNode->GetName()));
	}
	
#endif

	InNode->MarkNodePending();
}

void AJointActor::RequestNodeEndPlay(UJointNodeBase* InNode)
{
	if (!InNode) return;

	if (InNode->IsNodeEndedPlay()) return;

#if DEBUG_ShowNodeEvent_EndPlay
	
	if(GetWorld() && UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, FString::Printf(
												 TEXT("%s, %s, %s: RequestNodeEndPlay, Node : %s"),
												 UKismetSystemLibrary::IsStandalone(this)
													 ? *FString("Standalone")
													 : UKismetSystemLibrary::IsDedicatedServer(this)
													 ? *FString("Dedicated Server")
													 : UKismetSystemLibrary::IsServer(this)
													 ? *FString("Listen Server (Client_Host)")
													 : *FString("Client"),
												 *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
												 *this->GetName(), *InNode->GetName()));

		UE_LOG(LogTemp, Log, TEXT("%s"), *FString::Printf(TEXT("%s, %s, %s: RequestNodeEndPlay, Node : %s"),
				   UKismetSystemLibrary::IsStandalone(this)
				   ? *FString("Standalone")
				   : UKismetSystemLibrary::IsDedicatedServer(this)
				   ? *FString("Dedicated Server")
				   : UKismetSystemLibrary::IsServer(this)
				   ? *FString("Listen Server (Client_Host)")
				   : *FString("Client"),
				   *UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetName(),
				   *this->GetName(),
				   *InNode->GetName()));
	}
		
#endif

	KnownActiveNodes.Remove(InNode);

	InNode->NodeEndPlay();

#if WITH_EDITOR

	if (FJointModule* Module = &FModuleManager::GetModuleChecked<FJointModule>("Joint"); Module != nullptr && Module->
		OnJointDebuggerMentionNodeEndPlay.IsBound())
	{
		Module->OnJointDebuggerMentionNodeEndPlay.ExecuteIfBound(this, InNode);
	}

#endif
}

void AJointActor::NotifyNodeBeginPlay(UJointNodeBase* InNode)
{
	if (IsValidLowLevel() && OnJointNodeBeginPlayDelegate.IsBound()) OnJointNodeBeginPlayDelegate.Broadcast(this, InNode);
	//OnJointNodeBeginPlayDelegate.Broadcast(this, InNode);
}

void AJointActor::NotifyNodeEndPlay(UJointNodeBase* InNode)
{
	if (IsValidLowLevel() && OnJointNodeEndPlayDelegate.IsBound()) OnJointNodeEndPlayDelegate.Broadcast(this, InNode);
	//OnJointNodeEndPlayDelegate.Broadcast(this, InNode);
}

void AJointActor::NotifyNodePending(UJointNodeBase* InNode)
{
	if (IsValidLowLevel() && OnJointNodePendingDelegate.IsBound()) OnJointNodePendingDelegate.Broadcast(this, InNode);
	//OnJointNodePendingDelegate.Broadcast(this, InNode);
}

#if WITH_EDITOR

bool AJointActor::CheckDebuggerCanProceed(UJointNodeBase* InNode)
{
	if (FJointModule* Module = &FModuleManager::GetModuleChecked<FJointModule>("Joint"); Module != nullptr && Module->
		OnJointExecutionExceptionDelegate.IsBound())
	{
		return Module->OnJointExecutionExceptionDelegate.Execute(this, InNode);
	}

	return false;
}

#endif

void AJointActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	Params.Condition = COND_None;
	DOREPLIFETIME_WITH_PARAMS_FAST(AJointActor, CachedAllNodesForNetworking, Params);
}

bool AJointActor::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
#if USE_NEW_REPLICATION
	
	return Super::ReplicateSubobjects(Channel,Bunch,RepFlags);

#else


	check(Channel);
	check(Bunch);
	check(RepFlags);

	bool WroteSomething = false;


	for (UJointNodeBase* NodeForNetworking : CachedAllNodesForNetworking)
	{
		if (IsValid(NodeForNetworking))
		{
			WroteSomething |= Channel->ReplicateSubobject(NodeForNetworking, *Bunch, *RepFlags);
		}
	}

	for (UActorComponent* ActorComp : ReplicatedComponents)
	{
		if (ActorComp && ActorComp->GetIsReplicated())
		{
			WroteSomething |= ActorComp->ReplicateSubobjects(Channel, Bunch, RepFlags);
			// Lets the component add subobjects before replicating its own properties.
			WroteSomething |= Channel->ReplicateSubobject(ActorComp, *Bunch, *RepFlags);
			// (this makes those subobjects 'supported', and from here on those objects may have reference replicated)		
		}
	}

#if DEBUG_ShowReplication

	if (WroteSomething) UE_LOG(LogTemp, Log, TEXT("%s: Replicated subobjects"), *this->GetName());

#endif

	return WroteSomething;

#endif
}

void AJointActor::CacheNodesForNetworking()
{
	if (HasAuthority())
	{
		if (JointManager == nullptr) return;

#if USE_NEW_REPLICATION
		
		for (UJointNodeBase* NodesForNetworking : CachedAllNodesForNetworking)
		{
			if(IsValid(NodesForNetworking))
			{
				RemoveNodeForNetworking(NodesForNetworking);
			}
		}

#endif


		CachedAllNodesForNetworking.Empty();

		TArray<UJointNodeBase*> Nodes;

		for (UJointFragment* Fragment : JointManager->GetAllManagerFragmentsOnLowerHierarchy())
		{
			if (IsValid(Fragment))
			{
				if (Fragment->bReplicates)
				{
					Nodes.Add(Fragment);
				}
			}
		}

		for (UJointNodeBase* Node : JointManager->Nodes)
		{
			if (IsValid(Node))
			{
				if (Node->bReplicates)
				{
					Nodes.Add(Node);
				}

				for (UJointNodeBase* SubNodes : Node->GetAllFragmentsOnLowerHierarchy())
				{
					if (IsValid(SubNodes))
					{
						if (SubNodes->bReplicates)
						{
							Nodes.Add(SubNodes);
						}
					}
				}
			}
		}

		CachedAllNodesForNetworking = Nodes;

#if USE_NEW_REPLICATION
		
		for (UJointNodeBase* AllNodesForNetworking : CachedAllNodesForNetworking)
		{
			if(IsValid(AllNodesForNetworking))
			{
				AddReplicatedSubObject(AllNodesForNetworking);
			}
		}

#endif
	}
}

void AJointActor::AddNodeForNetworking(UJointNodeBase* InNode)
{
	if (HasAuthority())
	{
		if (JointManager == nullptr) return;

		if (InNode->GetHostingJointInstance() != this || InNode->GetJointManager() != this->GetJointManager())
			return;

		if (CachedAllNodesForNetworking.Contains(InNode)) return;

#if USE_NEW_REPLICATION
	
		AddReplicatedSubObject(InNode);

#endif

		CachedAllNodesForNetworking.Add(InNode);
	}
}

void AJointActor::RemoveNodeForNetworking(UJointNodeBase* InNode)
{
	if (HasAuthority())
	{
		if (JointManager == nullptr) return;

		if (InNode->GetHostingJointInstance() != this || InNode->GetJointManager() != this->GetJointManager())
			return;

		if (!CachedAllNodesForNetworking.Contains(InNode)) return;

#if USE_NEW_REPLICATION
	
		RemoveNodeForNetworking(InNode);

#endif

		CachedAllNodesForNetworking.Remove(InNode);
	}
}

#undef DEBUG_ShowNodeEvent
#undef DEBUG_ShowNodeEvent_BeginPlay
#undef DEBUG_ShowNodeEvent_EndPlay
#undef DEBUG_ShowNodeEvent_Pending

#undef DEBUG_ShowJointEvent
#undef DEBUG_ShowJointEvent_SetJoint
#undef DEBUG_ShowJointEvent_StartJoint
#undef DEBUG_ShowJointEvent_EndJoint
#undef DEBUG_ShowJointEvent_PlayNextNode
#undef DEBUG_ShowJointEvent_SetPlayingNode

#undef DEBUG_ShowReplication

#undef USE_NEW_REPLICATION
