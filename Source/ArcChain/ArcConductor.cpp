#include "ArcConductor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AArcConductor::AArcConductor()
{
	PrimaryActorTick.bCanEverTick = false;
	CubeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeMesh"));
	SetRootComponent(CubeMesh);
	CubeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		CubeMesh->SetStaticMesh(CubeAsset.Object);
	}
}
