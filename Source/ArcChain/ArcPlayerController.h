#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArcPlayerController.generated.h"

class AArcChainManager;

UCLASS()
class ARCCHAIN_API AArcPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void HandleSelectConductor();

	TWeakObjectPtr<AArcChainManager> ChainManager;
};
