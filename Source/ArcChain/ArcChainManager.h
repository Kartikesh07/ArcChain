#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ArcChainManager.generated.h"

class AArcConductor;

UCLASS()
class ARCCHAIN_API AArcChainManager : public AActor
{
	GENERATED_BODY()

public:
	AArcChainManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditInstanceOnly, Category = "Chain Lightning")
	TObjectPtr<AArcConductor> StartNode;

	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "0.0", Units = "cm"))
	float JumpRadius = 500.0f;

	// Number of connections, excluding the starting node.
	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "0"))
	int32 MaxJumps = 10;

	UPROPERTY(EditAnywhere, Category = "Chain Lightning", meta = (ClampMin = "0.01", Units = "s"))
	float JumpInterval = 0.25f;

	void PropagateNextJump();
	void StopPropagation();
	AArcConductor* FindNearestTarget(const AArcConductor& From) const;

	TArray<TWeakObjectPtr<AArcConductor>> Conductors;
	TSet<TWeakObjectPtr<AArcConductor>> VisitedNodes;
	TWeakObjectPtr<AArcConductor> CurrentNode;
	FTimerHandle JumpTimer;
	int32 CompletedJumps = 0;
};
