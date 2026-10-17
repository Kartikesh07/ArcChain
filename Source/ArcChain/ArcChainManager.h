#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ArcChainManager.generated.h"

class AArcConductor;
class UArcLightningComponent;

UCLASS()
class ARCCHAIN_API AArcChainManager : public AActor
{
	GENERATED_BODY()

public:
	AArcChainManager();

	// Invalid selections leave the current chain untouched.
	UFUNCTION(BlueprintCallable, Category = "Chain Lightning")
	void StartChain(AArcConductor* SelectedNode);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditInstanceOnly, Category = "Chain Lightning")
	TObjectPtr<AArcConductor> StartNode;

	UPROPERTY(VisibleAnywhere, Category = "Chain Lightning")
	TObjectPtr<UArcLightningComponent> LightningVisuals;

	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "0.0", Units = "cm"))
	float JumpRadius = 500.0f;

	// Total connections across all branches, excluding the starting node.
	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "0"))
	int32 MaxJumps = 10;

	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "0.01", Units = "s"))
	float JumpInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Chain Lightning")
	bool bEnableBranching = true;

	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "1", EditCondition = "bEnableBranching"))
	int32 MaxBranchesPerNode = 2;

	void PropagateNextJump();
	void StopPropagation();
	void ResetImpactFeedback();
	AArcConductor* FindNearestTarget(const AArcConductor& From) const;

	TArray<TWeakObjectPtr<AArcConductor>> Conductors;
	TSet<TWeakObjectPtr<AArcConductor>> VisitedNodes;
	TQueue<TWeakObjectPtr<AArcConductor>> PendingNodes;
	int32 PendingNodeCount = 0;
	FTimerHandle JumpTimer;
	int32 CompletedJumps = 0;
};
