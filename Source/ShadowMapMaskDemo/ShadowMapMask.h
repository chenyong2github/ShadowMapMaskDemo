// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShadowMapMask.generated.h"

UCLASS()
class SHADOWMAPMASKDEMO_API AShadowMapMask : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AShadowMapMask();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	bool GetStaticMeshLightMapCoordinateForHit(const UStaticMeshComponent* InStaticMeshComponent, int32 InTriangleIndex, const FVector& InHitLocation, FVector2D& OutLightMapUV);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
