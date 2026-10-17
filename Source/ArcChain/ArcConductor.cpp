#include "ArcConductor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName GlowColorParameter(TEXT("GlowColor"));
	const FName GlowIntensityParameter(TEXT("GlowIntensity"));
	const FName SparkCountParameter(TEXT("User.SparkCount"));
	const FName SparkColorParameter(TEXT("User.SparkColor"));
}

AArcConductor::AArcConductor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	CubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMesh"));
	SetRootComponent(CubeMesh);
	CubeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		CubeMesh->SetStaticMesh(CubeAsset.Object);
	}

	ImpactSparks = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ImpactSparks"));
	ImpactSparks->SetupAttachment(CubeMesh);
	ImpactSparks->SetAutoActivate(false);
	ImpactSparks->SetAutoDestroy(false);
}

void AArcConductor::PulseGlow()
{
	if (!HasActorBegunPlay() || IsActorBeingDestroyed() || !bConductive || !GlowMaterial
		|| !FMath::IsFinite(PulseDuration) || PulseDuration <= 0.0f
		|| !FMath::IsFinite(GlowIntensity) || GlowIntensity <= 0.0f)
	{
		return;
	}

	if (!GlowMID)
	{
		GlowMID = UMaterialInstanceDynamic::Create(GlowMaterial, this);
	}
	if (!GlowMID)
	{
		return;
	}

	// Preserve the base material and any pre-existing overlay through the pulse.
	if (CubeMesh->GetOverlayMaterial() != GlowMID.Get())
	{
		OriginalOverlayMaterial = CubeMesh->GetOverlayMaterial();
	}
	GlowMID->SetVectorParameterValue(GlowColorParameter, GlowColor);
	GlowMID->SetScalarParameterValue(GlowIntensityParameter, GlowIntensity);
	CubeMesh->SetOverlayMaterial(GlowMID);
	PulseStartTime = GetWorld()->GetTimeSeconds();
	ActivePulseDuration = PulseDuration;
	ActiveGlowIntensity = GlowIntensity;
	SetActorTickEnabled(true);
}

void AArcConductor::PlayImpactFeedback(const FVector& SourceLocation)
{
	if (!HasActorBegunPlay() || IsActorBeingDestroyed() || !bConductive)
	{
		return;
	}

	PulseGlow();
	ImpactSparks->DeactivateImmediate();
	UNiagaraSystem* SystemToPlay = SparkSystem ? SparkSystem.Get() : ImpactSparks->GetAsset();
	if (!IsValid(SystemToPlay) || SparkCount <= 0)
	{
		return;
	}

	// Place the burst just outside the struck face, rather than inside the opaque cube.
	FVector ImpactLocation;
	if (CubeMesh->GetClosestPointOnCollision(SourceLocation, ImpactLocation) > 0.0f)
	{
		ImpactLocation += (SourceLocation - ImpactLocation).GetSafeNormal() * 2.0f;
	}
	else
	{
		ImpactLocation = CubeMesh->Bounds.Origin
			+ FVector::UpVector * (CubeMesh->Bounds.BoxExtent.Z + 2.0f);
	}
	ImpactSparks->SetWorldLocation(ImpactLocation);
	if (ImpactSparks->GetAsset() != SystemToPlay)
	{
		ImpactSparks->SetAsset(SystemToPlay);
	}
	ImpactSparks->SetVariableInt(SparkCountParameter, SparkCount);
	ImpactSparks->SetVariableLinearColor(SparkColorParameter, SparkColor);
	ImpactSparks->Activate(true);
}

void AArcConductor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!GlowMID || ActivePulseDuration <= 0.0f)
	{
		FinishGlowPulse();
		return;
	}

	const float Alpha = FMath::Clamp(
		static_cast<float>(GetWorld()->GetTimeSeconds() - PulseStartTime) / ActivePulseDuration, 0.0f, 1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	GlowMID->SetScalarParameterValue(GlowIntensityParameter, ActiveGlowIntensity * (1.0f - SmoothAlpha));
	if (Alpha >= 1.0f)
	{
		FinishGlowPulse();
	}
}

void AArcConductor::FinishGlowPulse()
{
	if (GlowMID)
	{
		GlowMID->SetScalarParameterValue(GlowIntensityParameter, 0.0f);
		if (CubeMesh->GetOverlayMaterial() == GlowMID.Get())
		{
			CubeMesh->SetOverlayMaterial(OriginalOverlayMaterial);
		}
	}
	ActivePulseDuration = 0.0f;
	SetActorTickEnabled(false);
}

void AArcConductor::ResetImpactFeedback()
{
	FinishGlowPulse();
	ImpactSparks->DeactivateImmediate();
}

void AArcConductor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetImpactFeedback();
	Super::EndPlay(EndPlayReason);
}
