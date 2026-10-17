#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcShowcaseControls.generated.h"

class AArcChainManager;
class UBorder;
class USpinBox;
class UTextBlock;

UCLASS()
class ARCCHAIN_API UArcShowcaseControls : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetChainManager(AArcChainManager* Manager);
	bool IsPointerOverPanel() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	UFUNCTION()
	void HandleToggleMode();

	UFUNCTION()
	void HandleSettingChanged(float Value);

	UFUNCTION()
	void HandleReset();

	void ApplyPendingSettings();
	void RefreshPendingSettings();
	void RefreshStatus();

	TWeakObjectPtr<AArcChainManager> ChainManager;
	bool bNextBranching = true;
	bool bUpdatingControls = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StrikesText;

	UPROPERTY(Transient)
	TObjectPtr<USpinBox> BranchesInput;

	UPROPERTY(Transient)
	TObjectPtr<USpinBox> RadiusInput;

	UPROPERTY(Transient)
	TObjectPtr<USpinBox> IntervalInput;
};
