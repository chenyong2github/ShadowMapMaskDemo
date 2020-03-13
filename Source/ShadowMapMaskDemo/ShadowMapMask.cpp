#include "ShadowMapMask.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "LightMap.h"
#include "LightmapUniformShaderParameters.h"

// Sets default values
AShadowMapMask::AShadowMapMask()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

ACharacter* s_Character = nullptr;
// Called when the game starts or when spawned
void AShadowMapMask::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();

	// Find character actor
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		s_Character = *It;
		break;
	}

	// Create MIDs for character
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(s_Character->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
	const TArray<UMaterialInterface*> MaterialInterfaces = SkeletalMeshComponent->GetMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialInterfaces.Num(); ++MaterialIndex)
	{
		UMaterialInterface* MaterialInterface = MaterialInterfaces[MaterialIndex];

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MaterialInterface, this);
		SkeletalMeshComponent->SetMaterial(MaterialIndex, MID);
	}
}

// Called every frame
void AShadowMapMask::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector StartLocation = s_Character->GetActorLocation();
	FVector LineTrace = s_Character->GetActorLocation() + FVector(0, 0, -200);

	//UE_LOG(LogTemp, Warning, TEXT("Position:%f %f %f"), StartLocation.X, StartLocation.Y, StartLocation.Z)

	DrawDebugLine(GetWorld(), StartLocation, LineTrace, FColor::Red);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
	QueryParams.bReturnFaceIndex = true;

	GetWorld()->LineTraceSingleByObjectType(HitResult, StartLocation,
		LineTrace,
		FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic),
		QueryParams);

	AActor *HitObject = HitResult.GetActor();
	if (HitObject != nullptr)
	{
		UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(HitResult.Component.Get());
		if (HitStaticMeshComponent)
		{
			//Get ShadowMapMaskCoordinateScaleBias
			FPrimitiveSceneProxy* PrimitiveSceneProxy = HitStaticMeshComponent->SceneProxy;

			FPrimitiveSceneProxy::FLCIArray LCIs;
			PrimitiveSceneProxy->GetLCIs(LCIs);

			FVector4 CoordinateScaleBias;
			const FShadowMapInteraction ShadowMapInteraction = LCIs[0] ? LCIs[0]->GetShadowMapInteraction(GMaxRHIFeatureLevel) : FShadowMapInteraction();
			if (ShadowMapInteraction.GetType() == SMIT_Texture)
			{
				CoordinateScaleBias = FVector4(ShadowMapInteraction.GetCoordinateScale(), ShadowMapInteraction.GetCoordinateBias());
			}
			else
			{
				CoordinateScaleBias = FVector4(1, 1, 0, 0);
			}

			const FLightmapResourceCluster* LRC = LCIs[0]->GetResourceCluster();
			UTexture* ShadowMaskTexture = (UTexture*)LRC->Input.ShadowMapTexture;

			//Get LightMapCoordinate
			FVector2D LightMapCoordinate;
			if (GetStaticMeshLightMapCoordinateForHit(HitStaticMeshComponent, HitResult.FaceIndex, HitResult.Location, LightMapCoordinate))
			{
				UE_LOG(LogTemp, Warning, TEXT("Name: %s, FaceIndex: %d, LightMapUV: %f,%f"),
					*(HitObject->GetName()), HitResult.FaceIndex, LightMapCoordinate.X, LightMapCoordinate.Y);
			}
			else 
			{
				return;
			}

			USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(s_Character->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
			const TArray<UMaterialInterface*> MaterialInterfaces = SkeletalMeshComponent->GetMaterials();
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialInterfaces.Num(); ++MaterialIndex)
			{
				UMaterialInterface* MaterialInterface = MaterialInterfaces[MaterialIndex];

				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInterface);

				MID->SetTextureParameterValue(TEXT("StaticShadowTexture"), ShadowMaskTexture);

				MID->SetScalarParameterValue(TEXT("CoordinateScaleBias_X"), CoordinateScaleBias.X);
				MID->SetScalarParameterValue(TEXT("CoordinateScaleBias_Y"), CoordinateScaleBias.Y);
				MID->SetScalarParameterValue(TEXT("CoordinateScaleBias_Z"), CoordinateScaleBias.Z);
				MID->SetScalarParameterValue(TEXT("CoordinateScaleBias_W"), CoordinateScaleBias.W);

				MID->SetScalarParameterValue(TEXT("LightMapCoordinate_U"), LightMapCoordinate.X);
				MID->SetScalarParameterValue(TEXT("LightMapCoordinate_V"), LightMapCoordinate.Y);				
			}
		}
	}
}

bool AShadowMapMask::GetStaticMeshLightMapCoordinateForHit(const UStaticMeshComponent* InStaticMeshComponent, int32 InTriangleIndex, const FVector& InHitLocation, FVector2D& OutLightMapUV)
{
	const UStaticMesh* StaticMesh = Cast<UStaticMesh>(InStaticMeshComponent->GetStaticMesh());

	if (StaticMesh == nullptr || StaticMesh->RenderData == nullptr)
	{
		return false;
	}

	const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[0];
	const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = LODModel.VertexBuffers.StaticMeshVertexBuffer;

	if (StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		return false;
	}

	// Get the raw triangle data for this static mesh
	FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
	if (Indices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Check the AllowCPUAccess setting of the Static Mesh"));
		return false;
	}

	const FPositionVertexBuffer& PositionVertexBuffer = LODModel.VertexBuffers.PositionVertexBuffer;

	int32 SectionFirstTriIndex = 0;
	for (TArray<FStaticMeshSection>::TConstIterator SectionIt(LODModel.Sections); SectionIt; ++SectionIt)
	{
		const FStaticMeshSection& Section = *SectionIt;

		if (Section.bEnableCollision)
		{
			int32 NextSectionTriIndex = SectionFirstTriIndex + Section.NumTriangles;
			if (InTriangleIndex >= SectionFirstTriIndex && InTriangleIndex < NextSectionTriIndex)
			{

				int32 IndexBufferIdx = (InTriangleIndex - SectionFirstTriIndex) * 3 + Section.FirstIndex;

				// Look up the triangle vertex indices
				int32 Index0 = Indices[IndexBufferIdx];
				int32 Index1 = Indices[IndexBufferIdx + 1];
				int32 Index2 = Indices[IndexBufferIdx + 2];

				// Lookup the triangle world positions and uvs.
				FVector WorldVert0 = InStaticMeshComponent->GetComponentTransform().TransformPosition(PositionVertexBuffer.VertexPosition(Index0));
				FVector WorldVert1 = InStaticMeshComponent->GetComponentTransform().TransformPosition(PositionVertexBuffer.VertexPosition(Index1));
				FVector WorldVert2 = InStaticMeshComponent->GetComponentTransform().TransformPosition(PositionVertexBuffer.VertexPosition(Index2));

				FVector2D LightMapUV0;
				FVector2D LightMapUV1;
				FVector2D LightMapUV2;

				LightMapUV0 = StaticMeshVertexBuffer.GetVertexUV(Index0, 1);
				LightMapUV1 = StaticMeshVertexBuffer.GetVertexUV(Index1, 1);
				LightMapUV2 = StaticMeshVertexBuffer.GetVertexUV(Index2, 1);

				FVector BaryCoords = FMath::GetBaryCentric2D(InHitLocation, WorldVert0, WorldVert1, WorldVert2);

				FVector2D InterpLightMapUV = BaryCoords.X * LightMapUV0 + BaryCoords.Y * LightMapUV1 + BaryCoords.Z * LightMapUV2;

				OutLightMapUV = InterpLightMapUV;

				return true;
			}

			SectionFirstTriIndex = NextSectionTriIndex;
		}
	}

	return false;
}