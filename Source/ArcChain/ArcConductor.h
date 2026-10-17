#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcConductor.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS()
class ARCCHAIN_API AArcConductor : public AActor
{
	GENERATED_BODY()

public:
	AArcConductor();

	bool IsConductive() const { return bConductive; }

	void PulseGlow();
	void PlayImpactFeedback(const FVector& SourceLocation);
	void ResetImpactFeedback();

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void FinishGlowPulse();

	UPROPERTY(VisibleAnywhere, Category = "Conductor")
	TObjectPtr<UStaticMeshComponent> CubeMesh;

	UPROPERTY(EditAnywhere, Category = "Conductor")
	bool bConductive = true;

	UPROPERTY(VisibleAnywhere, Category = "Impact")
	TObjectPtr<UNiagaraComponent> ImpactSparks;

	// Additive overlay with GlowColor (vector) and GlowIntensity (scalar) parameters.
	UPROPERTY(EditAnywhere, Category = "Impact|Glow")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY(EditAnywhere, Category = "Impact|Glow", meta = (ClampMin = "0.0"))
	float GlowIntensity = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Impact|Glow", meta = (ClampMin = "0.01", Units = "s"))
	float PulseDuration = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Impact|Glow")
	FLinearColor GlowColor = FLinearColor(0.25f, 0.9f, 1.0f);

	// Optional override; otherwise use the system assigned to ImpactSparks.
	// Exposes User.SparkCount (int) and User.SparkColor (Linear Color).
	UPROPERTY(EditAnywhere, Category = "Impact|Sparks")
	TObjectPtr<UNiagaraSystem> SparkSystem;

	UPROPERTY(EditAnywhere, Category = "Impact|Sparks", meta = (ClampMin = "0"))
	int32 SparkCount = 20;

	UPROPERTY(EditAnywhere, Category = "Impact|Sparks")
	FLinearColor SparkColor = FLinearColor(0.4f, 0.95f, 1.0f);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalOverlayMaterial;

	double PulseStartTime = 0.0;
	float ActivePulseDuration = 0.0f;
	float ActiveGlowIntensity = 0.0f;
};
