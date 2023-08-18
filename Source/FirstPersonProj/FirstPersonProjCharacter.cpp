// Copyright Epic Games, Inc. All Rights Reserved.

#include "FirstPersonProjCharacter.h"
#include "FirstPersonProjProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/ArrowComponent.h"
#include "FPMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PawnMovementComponent.h"

//////////////////////////////////////////////////////////////////////////
// AFirstPersonProjCharacter

AFirstPersonProjCharacter::AFirstPersonProjCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Character doesnt have a rifle at start
	bHasRifle = false;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(FName(TEXT("Capsule Comp")));
	CapsuleComp->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComp->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	SetRootComponent(CapsuleComp);

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(CapsuleComp);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));

	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName ID_Characters;
		FText NAME_Characters;
		FConstructorStatics()
			: ID_Characters(TEXT("Characters"))
			, NAME_Characters(NSLOCTEXT("SpriteCategory", "Characters", "Characters"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	ArrowComp = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow Comp"));
	if (ArrowComp)
	{
		ArrowComp->ArrowColor = FColor(150, 200, 255);
		ArrowComp->bTreatAsASprite = true;
		ArrowComp->SpriteInfo.Category = ConstructorStatics.ID_Characters;
		ArrowComp->SpriteInfo.DisplayName = ConstructorStatics.NAME_Characters;
		ArrowComp->SetupAttachment(CapsuleComp);
		ArrowComp->bIsScreenSizeScaled = true;
		ArrowComp->SetSimulatePhysics(false);
	}
#endif // WITH_EDITORONLY_DATA

	MovementComponent = CreateDefaultSubobject<UFPMovementComponent>(FName(TEXT("PawnMovementComponent")));
	if (MovementComponent)
	{
		MovementComponent->UpdatedComponent = CapsuleComp;
	}

	Mesh3P = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(FName(TEXT("Mesh 3P")));
	if (Mesh3P)
	{
		Mesh3P->AlwaysLoadOnClient = true;
		Mesh3P->AlwaysLoadOnServer = true;
		Mesh3P->bOwnerNoSee = false;
		Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		Mesh3P->bCastDynamicShadow = true;
		Mesh3P->bAffectDynamicIndirectLighting = true;
		Mesh3P->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		Mesh3P->SetupAttachment(CapsuleComp);
		static FName MeshCollisionProfileName(TEXT("CharacterMesh"));
		Mesh3P->SetCollisionProfileName(MeshCollisionProfileName);
		Mesh3P->SetGenerateOverlapEvents(false);
		Mesh3P->SetCanEverAffectNavigation(false);
	}

	MoveIgnoreActorAdd(this);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
}

void AFirstPersonProjCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CachedBaseEyeHeight = BaseEyeHeight;
	BaseTranslationOffset = Mesh1P->GetRelativeLocation();
	BaseRotationOffset = Mesh1P->GetRelativeRotation().Quaternion();
	//MeshTranslationOffset = BaseTranslationOffset;
}

void AFirstPersonProjCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AFirstPersonProjCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Controller)
	{
		FRotator MeshRelativeRotation = Mesh1P->GetRelativeRotation();
		MeshRelativeRotation.Pitch = GetControlRotation().Pitch;
		Mesh1P->SetRelativeRotation(MeshRelativeRotation);
	}


	const UFPMovementComponent* FPComp = Cast<UFPMovementComponent>(MovementComponent);
	/*
	if (FPComp && FPComp->IsMovingOnGround())
	{
		const FVector UpVector = FPComp->GetCurrentFloorResult().HitResult.Normal;
		const FVector VelocityDir = FPComp->Velocity.GetSafeNormal2D();
		const FVector ForwardVec = UKismetMathLibrary::MakeRotFromZX(UpVector, VelocityDir).Vector();

		const FVector LineDrawStart = GetActorLocation() + (GetControlRotation().Vector() * 3.0f);
		const FVector LineDrawEnd = GetActorLocation() + (ForwardVec * 400.0f);
		DrawDebugLine(GetWorld(), LineDrawStart, LineDrawEnd, FColor::Green, false, 20.f, 0, .1f);
	}
	*/
	if (FPComp->IsMovingOnGround())
	{
		const FVector RampVector = FVector::VectorPlaneProject(FVector::DownVector, FPComp->GetCurrentFloorResult().HitResult.Normal);
		//UE_LOG(LogTemp, Warning, TEXT("Ramp vector: %s"), *RampVector.GetSafeNormal().ToString());
		//DrawDebugLine(GetWorld(), GetPawnFootLocation(), GetPawnFootLocation() + (RampVector * 400.0f), FColor::Green, false, 2.f, 0, .1f);
	}
}

//////////////////////////////////////////////////////////////////////////// Input

void AFirstPersonProjCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFirstPersonProjCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFirstPersonProjCharacter::Look);

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AFirstPersonProjCharacter::Jump);

		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AFirstPersonProjCharacter::CrouchPressed);

		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AFirstPersonProjCharacter::CrouchReleased);

		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AFirstPersonProjCharacter::SprintPressed);

		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AFirstPersonProjCharacter::SprintReleased);
	}
}

void AFirstPersonProjCharacter::OnCameraUpdate(const FVector& CameraLocation, const FRotator& CameraRotation)
{
	const FMatrix DefaultMeshLS = FRotationTranslationMatrix(BaseRotationOffset.Rotator(), GetMeshTranslationOffset());
	const FMatrix LocalToWorld = ActorToWorld().ToMatrixNoScale();

	const FRotator RotCameraPitch(CameraRotation.Pitch, 0.0f, 0.0f);
	const FRotator RotCameraYaw(0.0f, CameraRotation.Yaw, 0.0f);

	// Camera transform in local space relative to the pawn. Camera position and rotation relative to the actor.
	const FMatrix LeveledCameraLS = FRotationTranslationMatrix(RotCameraYaw, CameraLocation) * LocalToWorld.Inverse();
	// Local camera transform with pitch added back into it.
	const FMatrix PitchedCameraLS = FRotationMatrix(RotCameraPitch) * LeveledCameraLS;

	const FMatrix MeshRelativeToCamera = DefaultMeshLS * LeveledCameraLS.Inverse();
	const FMatrix PitchedMesh = MeshRelativeToCamera * PitchedCameraLS;

	Mesh1P->SetRelativeLocationAndRotation(PitchedMesh.GetOrigin(), PitchedMesh.Rotator());
}

void AFirstPersonProjCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AFirstPersonProjCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AFirstPersonProjCharacter::Jump(const FInputActionValue& Value)
{
	bWasJumpPressed = true;
}

void AFirstPersonProjCharacter::CrouchPressed(const FInputActionValue& Value)
{
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	check(MoveComp);
	MoveComp->SetWantsToCrouch(true);
}

void AFirstPersonProjCharacter::CrouchReleased(const FInputActionValue& Value)
{
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	check(MoveComp);
	MoveComp->SetWantsToCrouch(false);
}

void AFirstPersonProjCharacter::SprintPressed(const FInputActionValue& Value)
{
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	check(MoveComp);
	MoveComp->SetWantsToSprint(true);
}

void AFirstPersonProjCharacter::SprintReleased(const FInputActionValue& Value)
{
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	check(MoveComp);
	MoveComp->SetWantsToSprint(false);
}

void AFirstPersonProjCharacter::OnJumped()
{
	--JumpsRemaining;
}

void AFirstPersonProjCharacter::OnLanded(const FHitResult& HitResult)
{
	JumpsRemaining = 1;
}

void AFirstPersonProjCharacter::OnCrouchChanged(bool bIsCrouching)
{
	RecalculateBaseEyeHeight();

	/*
	if (!bIsCrouching)
	{
		Mesh1P->SetRelativeLocation(BaseTranslationOffset);
		MeshTranslationOffset = BaseTranslationOffset;
	}
	else
	{
		UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
		const float MeshOffset = MoveComp->GetDefaultCapsuelHalfHeight() - MoveComp->GetCrouchedHalfHeight();
		FVector& MeshRelativeLoc = Mesh1P->GetRelativeLocation_DirectMutable();
		MeshRelativeLoc.Z -= MeshOffset;
		MeshTranslationOffset = MeshRelativeLoc;
	}
	*/
}

void AFirstPersonProjCharacter::RecalculateBaseEyeHeight()
{
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	check(MoveComp);
	BaseEyeHeight = FMath::Lerp(CachedBaseEyeHeight, CrouchEyeHeight, MoveComp->GetCrouchFrac());
}

FVector AFirstPersonProjCharacter::GetPawnViewLocation() const
{
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	check(MoveComp);
	if (MoveComp->IsFalling())
	{
		return GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	}

	const float StandingHeight = CachedBaseEyeHeight + MoveComp->GetDefaultCapsuelHalfHeight();
	const float CrouchHeight = CrouchEyeHeight + MoveComp->GetCrouchedHalfHeight();
	return (FVector::UpVector * FMath::Lerp(StandingHeight, CrouchHeight, MoveComp->GetCrouchFrac())) + GetPawnFootLocation();
}

FVector AFirstPersonProjCharacter::GetPawnFootLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, CapsuleComp->GetScaledCapsuleHalfHeight());
}

FVector AFirstPersonProjCharacter::GetMeshTranslationOffset() const
{
	FVector Offset = BaseTranslationOffset;
	UFPMovementComponent* MoveComp = Cast<UFPMovementComponent>(MovementComponent);
	if (MoveComp)
	{
		const float CrouchOffset = (MoveComp->GetDefaultCapsuelHalfHeight() - MoveComp->GetCrouchedHalfHeight()) * MoveComp->GetCrouchFrac();
		Offset -= FVector::UpVector * CrouchOffset;
	}

	return Offset;
	
}

bool AFirstPersonProjCharacter::CanCharacterJump() const
{
	return JumpsRemaining > 0;
}

bool AFirstPersonProjCharacter::ConsumeJumpInput()
{
	bool bReturnJump = bWasJumpPressed;
	bWasJumpPressed = false;
	return bReturnJump;
}

void AFirstPersonProjCharacter::SetHasRifle(bool bNewHasRifle)
{
	bHasRifle = bNewHasRifle;
}

bool AFirstPersonProjCharacter::GetHasRifle()
{
	return bHasRifle;
}