// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "FPPCameraManager.generated.h"

/**
 * 
 */
UCLASS()
class FIRSTPERSONPROJ_API AFPPCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	
	virtual void UpdateCamera(float DeltaTime) override;
	
};
