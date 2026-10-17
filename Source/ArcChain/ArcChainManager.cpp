#include "ArcChainManager.h"

#include "ArcConductor.h"
#include "ArcLightningComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcChain, Log, All);

AArcChainManager::AArcChainManager()
{
	PrimaryActorTick.bCanEverTick = false;
	LightningVisuals = CreateDefaultSubobject<UArcLightningComponent>(TEXT("LightningVisuals"));
	SetRootComponent(LightningVisuals);
}

void AArcChainManager::StartChain(AArcConductor* SelectedNode)
{
	if (!HasActorBegunPlay() || IsActorBeingDestroyed() || !IsValid(SelectedNode)
		|| SelectedNode->IsActorBeingDestroyed() || SelectedNode->GetWorld() != GetWorld()
		|| !SelectedNode->IsConductive())
	{
		return;
	}

	ResetChain();
	StartNode = SelectedNode;
	VisitedNodes.Add(TWeakObjectPtr<AArcConductor>(SelectedNode));
	SelectedNode->PulseGlow();
	ActiveJumpRadius = JumpRadius;
	ActiveMaxJumps = MaxJumps;
	ActiveBranchLimit = bEnableBranching ? FMath::Clamp(MaxBranchesPerNode, 1, 4) : 1;

	if (MaxJumps <= 0
		|| !FMath::IsFinite(JumpRadius) || JumpRadius <= 0.0f
		|| !FMath::IsFinite(JumpInterval) || JumpInterval <= 0.0f)
	{
		PropagationStatus = EArcPropagationStatus::Complete;
		OnStateChanged.Broadcast();
		return;
	}

	// Refresh for each chain, including conductors spawned since the last selection.
	for (TActorIterator<AArcConductor> It(GetWorld()); It; ++It)
	{
		Conductors.Add(*It);
	}

	PendingNodes.Enqueue(TWeakObjectPtr<AArcConductor>(SelectedNode));
	PendingNodeCount = 1;
	GetWorldTimerManager().SetTimer(JumpTimer, this,
		&AArcChainManager::PropagateNextJump, JumpInterval, true);
	PropagationStatus = EArcPropagationStatus::Active;
	OnStateChanged.Broadcast();
}

void AArcChainManager::ConfigureNextChain(bool bBranching, int32 Branches, float Radius, float Interval)
{
	if (!FMath::IsFinite(Radius) || !FMath::IsFinite(Interval))
	{
		return;
	}
	bEnableBranching = bBranching;
	MaxBranchesPerNode = FMath::Clamp(Branches, 1, 4);
	JumpRadius = FMath::Clamp(Radius, 50.0f, 3000.0f);
	JumpInterval = FMath::Clamp(Interval, 0.05f, 2.0f);
}

void AArcChainManager::ResetChain()
{
	StopPropagation();
	LightningVisuals->ClearArcs();
	ResetImpactFeedback();
	Conductors.Reset();
	VisitedNodes.Reset();
	StartNode = nullptr;
	CompletedJumps = 0;
	ActiveJumpRadius = 0.0f;
	ActiveMaxJumps = 0;
	ActiveBranchLimit = 1;
	PropagationStatus = EArcPropagationStatus::Idle;
	OnStateChanged.Broadcast();
}

AArcConductor* AArcChainManager::FindNearestTarget(const AArcConductor& From) const
{
	AArcConductor* Nearest = nullptr;
	double NearestDistanceSquared = FMath::Square(static_cast<double>(ActiveJumpRadius));
	for (const TWeakObjectPtr<AArcConductor>& Candidate : Conductors)
	{
		AArcConductor* Target = Candidate.Get();
		if (!IsValid(Target) || Target->IsActorBeingDestroyed()
			|| !Target->IsConductive() || VisitedNodes.Contains(Candidate))
		{
			continue;
		}

		const double DistanceSquared = FVector::DistSquared(From.GetActorLocation(), Target->GetActorLocation());
		if (DistanceSquared <= NearestDistanceSquared)
		{
			Nearest = Target;
			NearestDistanceSquared = DistanceSquared;
		}
	}
	return Nearest;
}

void AArcChainManager::PropagateNextJump()
{
	if (CompletedJumps >= ActiveMaxJumps || PendingNodes.IsEmpty())
	{
		StopPropagation();
		PropagationStatus = EArcPropagationStatus::Complete;
		OnStateChanged.Broadcast();
		return;
	}

	// Process only the current frontier. Newly struck nodes wait until the next interval.
	const int32 FrontierSize = PendingNodeCount;
	for (int32 NodeIndex = 0; NodeIndex < FrontierSize && CompletedJumps < ActiveMaxJumps; ++NodeIndex)
	{
		TWeakObjectPtr<AArcConductor> QueuedNode;
		if (!PendingNodes.Dequeue(QueuedNode))
		{
			break;
		}
		--PendingNodeCount;
		AArcConductor* From = QueuedNode.Get();
		if (!IsValid(From) || From->IsActorBeingDestroyed() || !From->IsConductive())
		{
			// Losing one branch must not cancel other queued branches.
			continue;
		}

		for (int32 BranchIndex = 0; BranchIndex < ActiveBranchLimit && CompletedJumps < ActiveMaxJumps; ++BranchIndex)
		{
			AArcConductor* Target = FindNearestTarget(*From);
			if (!Target)
			{
				break;
			}

			// Claim immediately so no other parent can strike or enqueue this target again.
			const TWeakObjectPtr<AArcConductor> TargetNode(Target);
			VisitedNodes.Add(TargetNode);
			PendingNodes.Enqueue(TargetNode);
			++PendingNodeCount;
			++CompletedJumps;
			LightningVisuals->AddArc(From, Target);
			Target->PlayImpactFeedback(From->GetActorLocation());
			UE_LOG(LogArcChain, Log, TEXT("Jump %d: %s -> %s"), CompletedJumps,
				*GetNameSafe(From), *GetNameSafe(Target));
		}
	}

	if (CompletedJumps >= ActiveMaxJumps || PendingNodes.IsEmpty())
	{
		StopPropagation();
		PropagationStatus = EArcPropagationStatus::Complete;
	}
	OnStateChanged.Broadcast();
}

void AArcChainManager::StopPropagation()
{
	GetWorldTimerManager().ClearTimer(JumpTimer);
	PendingNodes.Empty();
	PendingNodeCount = 0;
}

void AArcChainManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetChain();
	Super::EndPlay(EndPlayReason);
}

void AArcChainManager::ResetImpactFeedback()
{
	for (const TWeakObjectPtr<AArcConductor>& Node : VisitedNodes)
	{
		if (AArcConductor* Conductor = Node.Get(); IsValid(Conductor) && !Conductor->IsActorBeingDestroyed())
		{
			Conductor->ResetImpactFeedback();
		}
	}
}
