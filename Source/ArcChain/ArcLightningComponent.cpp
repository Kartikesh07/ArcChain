#include "ArcLightningComponent.h"

#include "ArcConductor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcLightning, Log, All);

namespace
{
	// Engine/BasicShapes/Cylinder is 100 cm long on Z and 100 cm in diameter.
	constexpr float CylinderSize = 100.0f;
	const FTransform HiddenInstance(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	const FName ArcColorParameter(TEXT("ArcColor"));
}

UArcLightningComponent::UArcLightningComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.05f;
	SetMobility(EComponentMobility::Movable);
	SetCollisionProfileName(TEXT("NoCollision"));
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetCastShadow(false);
	SetNumCustomDataFloats(1);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		SetStaticMesh(CylinderAsset.Object);
	}
}

void UArcLightningComponent::BeginPlay()
{
	Super::BeginPlay();
	if (LightningMaterial)
	{
		LightningMID = CreateDynamicMaterialInstance(0, LightningMaterial);
	}
	else
	{
		UE_LOG(LogArcLightning, Warning, TEXT("%s: assign Lightning Material on the Lightning Visuals component to display arcs."),
			*GetNameSafe(GetOwner()));
	}
}

void UArcLightningComponent::AddArc(AArcConductor* From, AArcConductor* To)
{
	if (!LightningMID || !GetStaticMesh() || !IsValid(From) || !IsValid(To)
		|| From->IsActorBeingDestroyed() || To->IsActorBeingDestroyed()
		|| !FMath::IsFinite(Duration) || Duration <= 0.0f
		|| !FMath::IsFinite(Deviation) || !FMath::IsFinite(Thickness) || Thickness <= 0.0f)
	{
		return;
	}

	FActiveArc& Arc = ActiveArcs.AddDefaulted_GetRef();
	Arc.From = From;
	Arc.To = To;
	Arc.StartTime = GetWorld()->GetTimeSeconds();
	Arc.Lifetime = Duration;
	Arc.Offset = FMath::Max(0.0f, Deviation);
	Arc.Width = Thickness;
	const int32 SegmentCount = FMath::Clamp(Segments, 2, 64);
	Arc.InstanceIndices.Reserve(SegmentCount);
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		// Grow only when concurrent arcs exceed the pool's previous high-water mark.
		const int32 InstanceIndex = FreeInstances.IsEmpty()
			? AddInstance(HiddenInstance) : FreeInstances.Pop(EAllowShrinking::No);
		Arc.InstanceIndices.Add(InstanceIndex);
	}

	LightningMID->SetVectorParameterValue(ArcColorParameter, ArcColor);
	UpdateArc(Arc, 1.0f);
	MarkRenderStateDirty();
	SetComponentTickEnabled(true);
}

void UArcLightningComponent::UpdateArc(const FActiveArc& Arc, float Fade)
{
	const FVector Start = Arc.From->GetActorLocation();
	const FVector End = Arc.To->GetActorLocation();
	const FVector Direction = (End - Start).GetSafeNormal();
	FVector Side, Up;
	Direction.FindBestAxisVectors(Side, Up);
	const int32 SegmentCount = Arc.InstanceIndices.Num();
	const float Opacity = Fade * FMath::FRandRange(0.65f, 1.0f);
	FVector PreviousPoint = Start;

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float Alpha = static_cast<float>(Index + 1) / SegmentCount;
		FVector NextPoint = End;
		if (Index + 1 < SegmentCount)
		{
			// Offset only interior points, tapering displacement towards the endpoints.
			const float Envelope = FMath::Sin(PI * Alpha) * Arc.Offset;
			NextPoint = FMath::Lerp(Start, End, Alpha) + Envelope
				* (Side * FMath::FRandRange(-1.0f, 1.0f) + Up * FMath::FRandRange(-1.0f, 1.0f));
		}

		const FVector Delta = NextPoint - PreviousPoint;
		const double Length = Delta.Size();
		const FQuat Rotation = Length > UE_SMALL_NUMBER
			? FQuat::FindBetweenNormals(FVector::UpVector, Delta / Length) : FQuat::Identity;
		const FTransform Transform(Rotation, (PreviousPoint + NextPoint) * 0.5,
			FVector(Arc.Width / CylinderSize, Arc.Width / CylinderSize, Length / CylinderSize));
		UpdateInstanceTransform(Arc.InstanceIndices[Index], Transform, true, false, true);
		SetCustomDataValue(Arc.InstanceIndices[Index], 0, Opacity, false);
		PreviousPoint = NextPoint;
	}
}

void UArcLightningComponent::ReleaseArc(const FActiveArc& Arc)
{
	for (const int32 InstanceIndex : Arc.InstanceIndices)
	{
		UpdateInstanceTransform(InstanceIndex, HiddenInstance, false, false, true);
		SetCustomDataValue(InstanceIndex, 0, 0.0f, false);
		FreeInstances.Add(InstanceIndex);
	}
}

void UArcLightningComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = ActiveArcs.Num() - 1; Index >= 0; --Index)
	{
		const FActiveArc& Arc = ActiveArcs[Index];
		const float Age = static_cast<float>(Now - Arc.StartTime);
		if (Age >= Arc.Lifetime || !Arc.From.IsValid() || !Arc.To.IsValid()
			|| Arc.From->IsActorBeingDestroyed() || Arc.To->IsActorBeingDestroyed())
		{
			ReleaseArc(Arc);
			ActiveArcs.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
		else
		{
			UpdateArc(Arc, FMath::Clamp(1.0f - Age / Arc.Lifetime, 0.0f, 1.0f));
		}
	}
	MarkRenderStateDirty();
	if (ActiveArcs.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

void UArcLightningComponent::ClearArcs()
{
	for (const FActiveArc& Arc : ActiveArcs)
	{
		ReleaseArc(Arc);
	}
	ActiveArcs.Reset();
	MarkRenderStateDirty();
	SetComponentTickEnabled(false);
}
