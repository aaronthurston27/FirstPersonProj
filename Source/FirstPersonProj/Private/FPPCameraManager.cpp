#include "FPPCameraManager.h"
#include "FirstPersonProj/FirstPersonProjCharacter.h"
// Fill out your copyright notice in the Description page of Project Settings.


void AFPPCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);

	if (PCOwner)
	{
		AFirstPersonProjCharacter* FPPCharacter = Cast<AFirstPersonProjCharacter>(PCOwner->GetPawn());
		if (FPPCharacter)
		{
			FPPCharacter->OnCameraUpdate(GetCameraLocation(), GetCameraRotation());
		}
	}
}
