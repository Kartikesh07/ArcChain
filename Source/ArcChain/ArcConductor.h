#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcConductor.generated.h"

class UStaticMeshComponent;

UCLASS()
class ARCCHAIN_API AArcConductor : public AActor
{
	GENERATED_BODY()

public:
	AArcConductor();

	bool IsConductive() const { return bConductive; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Conductor")
	TObjectPtr<UStaticMeshComponent> CubeMesh;

	UPROPERTY(EditAnywhere, Category = "Conductor")
	bool bConductive = true;
};
