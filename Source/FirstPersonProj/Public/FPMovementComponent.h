// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "CharacterMovementComponentAsync.h"
#include "FPMovementComponent.generated.h"

class AFirstPersonProjCharacter;

/** Movement modes for first person character */
UENUM(BlueprintType)
enum EFPMovementMode : int
{
	/** None (movement is disabled). */
	None		UMETA(DisplayName = "None"),

	/** Walking on a surface. */
	Walking	UMETA(DisplayName = "Walking"),

	/** Sliding along a surface */
	Sliding	UMETA(DisplayName = "Sliding"),

	/**
	 * Simplified walking on navigation data (e.g. navmesh).
	 * If GetGenerateOverlapEvents() is true, then we will perform sweeps with each navmesh move.
	 * If GetGenerateOverlapEvents() is false then movement is cheaper but characters can overlap other objects without some extra process to repel/resolve their collisions.
	 */
	NavWalking	UMETA(DisplayName = "Navmesh Walking"),

	/** Falling under the effects of gravity, such as after jumping or walking off the edge of a surface. */
	Falling	UMETA(DisplayName = "Falling"),

	MAX		UMETA(Hidden),
};

/**
 * Custom first person movement component
 */
UCLASS()
class FIRSTPERSONPROJ_API UFPMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:

	UFPMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;

	virtual void BeginPlay() override;

	virtual void InitializeComponent() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	AFirstPersonProjCharacter* GetFPPOwner() const;

public:

	/** Get the max angle in degrees of a walkable surface for the character. */
	UFUNCTION(BlueprintPure)
	float GetWalkableFloorAngle() const;

	/** Set the max angle in degrees of a walkable surface for the character. Also computes WalkableFloorZ. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|CharacterMovement")
	void SetWalkableFloorAngle(float InWalkableFloorAngle);

	/** Get the Z component of the normal of the steepest walkable surface for the character. Any lower than this and it is not walkable. */
	UFUNCTION(BlueprintPure, Category = "Pawn|Components|CharacterMovement", meta = (DisplayName = "GetWalkableFloorZ", ScriptName = "GetWalkableFloorZ"))
	float GetWalkableFloorZ() const;

	/** Set the Z component of the normal of the steepest walkable surface for the character. Also computes WalkableFloorAngle. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|CharacterMovement")
	void SetWalkableFloorZ(float InWalkableFloorZ);

public:

	/** Saved location of object we are standing on, for UpdateBasedMovement() to determine if base moved in the last frame, and therefore pawn needs an update. */
	FQuat OldBaseQuat;

	/** Saved location of object we are standing on, for UpdateBasedMovement() to determine if base moved in the last frame, and therefore pawn needs an update. */
	FVector OldBaseLocation;

private:

	/**
	 * Actor's current movement mode (walking, falling, etc).
	 *    - walking:  Walking on a surface, under the effects of friction, and able to "step up" barriers. Vertical velocity is zero.
	 *    - falling:  Falling under the effects of gravity, after jumping or walking off the edge of a surface.
	 *    - custom:   User-defined custom movement mode, including many possible sub-modes.
	 * This is automatically replicated through the Character owner and for client-server movement functions.
	 * @see SetMovementMode(), CustomMovementMode
	 */
	UPROPERTY(Transient)
	TEnumAsByte<enum EFPMovementMode> MovementMode;

protected:

	UPROPERTY(Transient)
	AFirstPersonProjCharacter* CachedOwnerChar = nullptr;


protected:

	void PerformMovement(const float DeltaTime);

	void PerformWalkMovement(const float DeltaTime, const FVector& InputVector);

	void PerformSlideMovement(const float DeltaTime, const FVector& InputVector);

	void PerformFallMovement(const float DeltaTime, const FVector& InputVector);

protected:

	// Walk/Ground movement

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float GroundFriction;

	/** Max Acceleration (rate of change of velocity) while walking. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float WalkAcceleration;

	/** Max Acceleration (rate of change of velocity) while sprinting. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float SprintAcceleration;

	/**
	 * Max angle in degrees of a walkable surface. Any greater than this and it is too steep to be walkable.
	 */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "90.0", UIMin = "0.0", UIMax = "90.0", ForceUnits = "degrees"))
	float WalkableFloorAngle;

	/**
	 * Minimum Z value for floor normal. If less, not a walkable surface. Computed from WalkableFloorAngle.
	 */
	UPROPERTY(Category = "Character Movement: Walking", VisibleAnywhere)
	float WalkableFloorZ;

	/** Maximum height character can step up */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm"))
	float MaxStepHeight;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float BrakingDecelerationWalking;

	/** The maximum ground speed when walking. Also determines maximum lateral speed when falling. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxWalkSpeed;

	/** The maximum ground speed when sprinting. Also determines maximum lateral speed when falling. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSprintSpeed;

	/** The maximum ground speed when crouched. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSpeedCrouched;

	/** The maximum speed while sliding. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxSlideSpeed;

	/** Information about the floor the Character is standing on (updated only during walking movement). */
	UPROPERTY(Category = "Character Movement: Walking", VisibleInstanceOnly, BlueprintReadOnly)
	FFindFloorResult CurrentFloor;

	UPROPERTY(Transient)
	bool bWantsToSprint = false;

	UPROPERTY(Transient)
	bool bIsSprinting = false;

	void CalculateGroundVelocity(const FVector& InputVector, float DeltaTime);

	UFUNCTION(BlueprintPure)
	bool CanSprint(const FVector& InputVector) const;

	void SetIsSprinting(bool bNewIsSprinting);

	void StartGroundMovement();

	void OnGroundMovementStopped();

	void FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult) const;

	bool IsWalkableSurface(const FHitResult& FloorHitResult) const;

	/** Returns true if we can step up on the actor in the given FHitResult. */
	virtual bool CanStepUp(const FHitResult& Hit) const;

	/** Custom version of SlideAlongSurface that handles different movement modes separately; namely during walking physics we might not want to slide up slopes. */
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;

	/**
	 * Move up steps or slope. Does nothing and returns false if CanStepUp(Hit) returns false.
	 *
	 * @param GravDir			Gravity vector direction (assumed normalized or zero)
	 * @param Delta				Requested move
	 * @param Hit				[In] The hit before the step up.
	 * @param OutStepDownResult	[Out] If non-null, a floor check will be performed if possible as part of the final step down, and it will be updated to reflect this result.
	 * @return true if the step up was successful.
	 */
	virtual bool StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& Hit, FStepDownResult* OutStepDownResult = NULL);

	/**
	 * Determine whether we should try to find a valid landing spot after an impact with an invalid one (based on the Hit result).
	 * For example, landing on the lower portion of the capsule on the edge of geometry may be a walkable surface, but could have reported an unwalkable impact normal.
	 */
	virtual bool ShouldCheckForValidLandingSpot(float DeltaTime, const FHitResult& Hit) const;

	/**
	 * Return true if the 2D distance to the impact point is inside the edge tolerance (CapsuleRadius minus a small rejection threshold).
	 * Useful for rejecting adjacent hits when finding a floor or landing spot.
	 */
	virtual bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const;

protected:

	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxAirSpeed;

	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxAirStrafe;

	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float AirAcceleration;

	/** Air braking acceleration */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float AirBrakingDeceleration;

	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0" , ClampMax = "1", UIMax = "1"))
	float AirFrictionFactor = 1.0f;

	/** Custom gravity scale. Gravity is multiplied by this amount for the character. */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite)
	float GravityScale;

	/** Initial velocity (instantaneous vertical acceleration) when jumping. */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Jump Z Velocity", ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float JumpZVelocity;

	/** Grace period from the time the player starts falling that a jump can be initated. */
	UPROPERTY(Category = "Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float JumpGracePeriod = .35f;

	UPROPERTY(Transient)
	float TimeFallStartedSeconds = 0.0f;

	UPROPERTY(Transient)
	FVector InitialJumpVelocity = FVector::ZeroVector;

	void StartFalling();

	void CalculateFallVelocity(const FVector& InputVector, float DeltaTime);

	bool CanJump();

	void DoJump();

	void OnFallMovementStopped();

public:

	UFUNCTION(BlueprintPure)
	bool IsSprinting() const;

	void SetWantsToSprint(bool bWantsToSprint);

	virtual bool IsFalling() const override;

	virtual bool IsMovingOnGround() const override;

	virtual float GetGravityZ() const override;

protected:

	UPROPERTY(EditDefaultsOnly)
	float CapsuleCrouchHalfHeight = 40.0f;

	UPROPERTY(EditDefaultsOnly)
	float TimeToCrouchSeconds = .3f;

	UPROPERTY(Transient)
	float CachedDefaultCapsuleHalfHeight;

	UPROPERTY(Transient)
	bool bWantsToCrouch = false;

	UPROPERTY(Transient)
	float CrouchFrac = 0.0f;

	UPROPERTY(Transient)
	bool bIsCrouched = false;

	void TickCrouch(float DeltaTime);

	bool CanCharacterUncrouch() const;

public:

	void SetWantsToCrouch(bool bWantsToCrouch);

	UFUNCTION(BlueprintPure)
	bool CanCrouch() const;

	bool IsCrouching() const override;

	float GetCrouchFrac() const;

	float GetCrouchedHalfHeight() const;

	float GetDefaultCapsuelHalfHeight() const;

protected:

	void SetMovementMode(EFPMovementMode NewMovementMode);

	void OnMovementModeChanged(EFPMovementMode OldMovementMode, EFPMovementMode NewMovementMode);
	
public:

	/** Minimum acceptable distance for Character capsule to float above floor when walking. */
	static const float MIN_FLOOR_DIST;

	/** Maximum acceptable distance for Character capsule to float above floor when walking. */
	static const float MAX_FLOOR_DIST;

	/** Amount to shrink capsule by when sweeping against the floor */
	static const float CAPSULE_RADIUS_SHRINK_FACTOR;

	/** Reject sweep impacts that are this close to the edge of the vertical portion of the capsule when performing vertical sweeps, and try again with a smaller capsule. */
	static const float SWEEP_EDGE_REJECT_DISTANCE;
	
};
