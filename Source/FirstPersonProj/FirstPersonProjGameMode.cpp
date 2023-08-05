// Copyright Epic Games, Inc. All Rights Reserved.

#include "FirstPersonProjGameMode.h"
#include "FirstPersonProjCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFirstPersonProjGameMode::AFirstPersonProjGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
