#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArcPlayerController.generated.h"

class AArcChainManager;
class UArcShowcaseControls;

UCLASS()
class ARCCHAIN_API AArcPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleSelectConductor();
	AArcChainManager* ResolveChainManager();

	TWeakObjectPtr<AArcChainManager> ChainManager;

	UPROPERTY(Transient)
	TObjectPtr<UArcShowcaseControls> ShowcaseControls;
};
