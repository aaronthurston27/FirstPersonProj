// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Engine/EngineTypes.h"
#include "FirstPersonProjCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCapsuleComponent;
class USceneComponent;
class UCameraComponent;
class UAnimMontage;
class USoundBase;
class UArrowComponent;

UCLASS(config=Game)
class AFirstPersonProjCharacter : public APawn
{
	GENERATED_BODY()

protected:

	/** The CapsuleComponent being used for movement collision (by CharacterMovement). Always treated as being vertically aligned in simple collision check functions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* CapsuleComp;

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh3P)
	USkeletalMeshComponent* Mesh1P;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	class UInputAction* MoveAction;

	/** Crouch Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* CrouchAction;

	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* SprintAction;

	/** The main skeletal mesh associated with this Character (optional sub-object). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* Mesh3P;

	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UPawnMovementComponent* MovementComponent;

#if WITH_EDITORONLY_DATA
	/** Component shown in the editor only to indicate character facing */
	UPROPERTY()
	UArrowComponent* ArrowComp;
#endif

public:
	AFirstPersonProjCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;

protected:
	virtual void BeginPlay();

	virtual void Tick(float DeltaTime) override;

public:
		
	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Bool for AnimBP to switch to another animation set */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bHasRifle;

	/** Setter to set the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void SetHasRifle(bool bNewHasRifle);

	/** Getter for the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	bool GetHasRifle();

	/** Returns Mesh subobject **/
	FORCEINLINE class USkeletalMeshComponent* GetMesh() const { return Mesh3P; }

	/** Name of the MeshComponent. Use this name if you want to prevent creation of the component (with ObjectInitializer.DoNotCreateDefaultSubobject). */
	static FName MeshComponentName;

	virtual UPawnMovementComponent* GetMovementComponent() const override { return MovementComponent; }

	/** Returns CharacterMovement subobject **/
	template <class T>
	FORCEINLINE_DEBUGGABLE T* GetCharacterMovement() const
	{
		return CastChecked<T>(MovementComponent, ECastCheckedType::NullAllowed);
	}
	FORCEINLINE UPawnMovementComponent* GetCharacterMovement() const { return MovementComponent; }

	/** Returns CapsuleComponent subobject **/
	FORCEINLINE class UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComp; }

	/** Name of the CapsuleComponent. */
	static FName CapsuleComponentName;

#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const { return ArrowComp; }
#endif

protected:

	UPROPERTY(Transient)
	bool bWasJumpPressed = false;

	UPROPERTY(Transient)
	float TimeJumpWasPressedSeconds = 0.0f;

	UPROPERTY(Transient)
	int32 JumpsRemaining = 1;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	void Jump(const FInputActionValue& Value);

	void CrouchPressed(const FInputActionValue& Value);

	void CrouchReleased(const FInputActionValue& Value);

	void SprintPressed(const FInputActionValue& Value);

	void SprintReleased(const FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly)
	float CrouchEyeHeight = 40.0f;

	UPROPERTY(Transient)
	float CachedBaseEyeHeight;

	/** Saved translation offset of mesh. */
	UPROPERTY(Transient)
	FVector BaseTranslationOffset;

	/** Saved rotation offset of mesh. */
	UPROPERTY(Transient)
	FQuat BaseRotationOffset;

	FVector GetMeshTranslationOffset() const;

public:

	bool CanCharacterJump() const;

	bool ConsumeJumpInput();

	void OnJumped();

	void OnLanded(const FHitResult& HitResult);

public:

	void OnCrouchChanged(bool bIsCrouching);

	virtual void RecalculateBaseEyeHeight() override;

	virtual FVector GetPawnViewLocation() const override;

	FVector GetPawnFootLocation() const;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	
	void OnCameraUpdate(const FVector& CameraLocation, const FRotator& CameraRotation);

};

