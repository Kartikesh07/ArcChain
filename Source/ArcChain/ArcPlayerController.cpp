#include "ArcPlayerController.h"

#include "ArcChainManager.h"
#include "ArcConductor.h"
#include "Components/InputComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

void AArcPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
}

void AArcPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,
			this, &AArcPlayerController::HandleSelectConductor);
	}
}

void AArcPlayerController::HandleSelectConductor()
{
	if (!IsLocalController())
	{
		return;
	}

	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(
		UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit))
	{
		return;
	}

	AArcConductor* SelectedNode = Cast<AArcConductor>(Hit.GetActor());
	if (!IsValid(SelectedNode) || SelectedNode->IsActorBeingDestroyed() || !SelectedNode->IsConductive())
	{
		return;
	}

	// The level needs one manager; reacquire safely if the cached manager was removed.
	if (!ChainManager.IsValid() || ChainManager->IsActorBeingDestroyed())
	{
		ChainManager.Reset();
		for (TActorIterator<AArcChainManager> It(GetWorld()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed())
			{
				ChainManager = *It;
				break;
			}
		}
	}

	if (AArcChainManager* Manager = ChainManager.Get())
	{
		Manager->StartChain(SelectedNode);
	}
}
