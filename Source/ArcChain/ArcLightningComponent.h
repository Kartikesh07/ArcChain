#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ArcLightningComponent.generated.h"

class AArcConductor;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// One instanced mesh and material shared by all active arcs; expired slots are recycled.
UCLASS(ClassGroup = (Rendering))
class ARCCHAIN_API UArcLightningComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UArcLightningComponent();

	void AddArc(AArcConductor* From, AArcConductor* To);
	void ClearArcs();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Additive, unlit material: ArcColor * Brightness -> Emissive;
	// PerInstanceCustomData[0] via VertexInterpolator -> Opacity.
	UPROPERTY(EditAnywhere, Category = "Lightning")
	TObjectPtr<UMaterialInterface> LightningMaterial;

	UPROPERTY(EditAnywhere, Category = "Lightning", meta = (ClampMin = "2", ClampMax = "64"))
	int32 Segments = 12;

	UPROPERTY(EditAnywhere, Category = "Lightning", meta = (ClampMin = "0.0", Units = "cm"))
	float Deviation = 30.0f;

	// Diameter of each cylinder segment.
	UPROPERTY(EditAnywhere, Category = "Lightning", meta = (ClampMin = "0.1", Units = "cm"))
	float Thickness = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Lightning")
	FLinearColor ArcColor = FLinearColor(0.65f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, Category = "Lightning", meta = (ClampMin = "0.05", Units = "s"))
	float Duration = 0.35f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LightningMID;

	struct FActiveArc
	{
		TWeakObjectPtr<AArcConductor> From;
		TWeakObjectPtr<AArcConductor> To;
		TArray<int32> InstanceIndices;
		double StartTime = 0.0;
		float Lifetime = 0.0f;
		float Offset = 0.0f;
		float Width = 0.0f;
	};

	void UpdateArc(const FActiveArc& Arc, float Fade);
	void ReleaseArc(const FActiveArc& Arc);

	TArray<FActiveArc> ActiveArcs;
	TArray<int32> FreeInstances;
};
