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

	StopPropagation();
	LightningVisuals->ClearArcs();
	ResetImpactFeedback();
	Conductors.Reset();
	VisitedNodes.Reset();
	CompletedJumps = 0;
	StartNode = SelectedNode;
	VisitedNodes.Add(TWeakObjectPtr<AArcConductor>(SelectedNode));
	SelectedNode->PulseGlow();

	if (MaxJumps <= 0
		|| !FMath::IsFinite(JumpRadius) || JumpRadius <= 0.0f
		|| !FMath::IsFinite(JumpInterval) || JumpInterval <= 0.0f)
	{
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
}

AArcConductor* AArcChainManager::FindNearestTarget(const AArcConductor& From) const
{
	AArcConductor* Nearest = nullptr;
	double NearestDistanceSquared = FMath::Square(static_cast<double>(JumpRadius));
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
	if (CompletedJumps >= MaxJumps || PendingNodes.IsEmpty())
	{
		StopPropagation();
		return;
	}

	const int32 BranchLimit = bEnableBranching ? FMath::Max(1, MaxBranchesPerNode) : 1;
	// Process only the current frontier. Newly struck nodes wait until the next interval.
	const int32 FrontierSize = PendingNodeCount;
	for (int32 NodeIndex = 0; NodeIndex < FrontierSize && CompletedJumps < MaxJumps; ++NodeIndex)
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

		for (int32 BranchIndex = 0; BranchIndex < BranchLimit && CompletedJumps < MaxJumps; ++BranchIndex)
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

	if (CompletedJumps >= MaxJumps || PendingNodes.IsEmpty())
	{
		StopPropagation();
	}
}

void AArcChainManager::StopPropagation()
{
	GetWorldTimerManager().ClearTimer(JumpTimer);
	PendingNodes.Empty();
	PendingNodeCount = 0;
}

void AArcChainManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPropagation();
	LightningVisuals->ClearArcs();
	ResetImpactFeedback();
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
