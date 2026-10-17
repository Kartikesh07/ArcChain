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
	Conductors.Reset();
	VisitedNodes.Reset();
	CurrentNode.Reset();
	CompletedJumps = 0;
	StartNode = SelectedNode;

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

	CurrentNode = StartNode.Get();
	VisitedNodes.Add(CurrentNode);
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
		if (!IsValid(Target) || !Target->IsConductive() || VisitedNodes.Contains(Candidate))
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
	AArcConductor* From = CurrentNode.Get();
	if (!IsValid(From) || !From->IsConductive() || CompletedJumps >= MaxJumps)
	{
		StopPropagation();
		return;
	}

	AArcConductor* Target = FindNearestTarget(*From);
	if (!Target)
	{
		StopPropagation();
		return;
	}

	VisitedNodes.Add(TWeakObjectPtr<AArcConductor>(Target));
	CurrentNode = Target;
	++CompletedJumps;
	LightningVisuals->AddArc(From, Target);
	UE_LOG(LogArcChain, Log, TEXT("Jump %d: %s -> %s"), CompletedJumps,
		*GetNameSafe(From), *GetNameSafe(Target));

	if (CompletedJumps >= MaxJumps)
	{
		StopPropagation();
	}
}

void AArcChainManager::StopPropagation()
{
	GetWorldTimerManager().ClearTimer(JumpTimer);
}

void AArcChainManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPropagation();
	LightningVisuals->ClearArcs();
	Super::EndPlay(EndPlayReason);
}
