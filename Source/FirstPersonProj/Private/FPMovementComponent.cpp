// Fill out your copyright notice in the Description page of Project Settings.


#include "FPMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "FirstPersonProj/FirstPersonProjCharacter.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/Character.h"

const float UFPMovementComponent::MIN_FLOOR_DIST = 1.9f;
const float UFPMovementComponent::MAX_FLOOR_DIST = 2.4f;
const float UFPMovementComponent::CAPSULE_RADIUS_SHRINK_FACTOR = .4f;
const float UFPMovementComponent::SWEEP_EDGE_REJECT_DISTANCE = 0.15f;

UFPMovementComponent::UFPMovementComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

	GravityScale = 1.f;
	GroundFriction = 8.0f;
	JumpZVelocity = 420.0f;
	SetWalkableFloorZ(0.71f);

	MaxStepHeight = 45.0f;

	MaxWalkSpeed = 600.0f;
	MaxSprintSpeed = 750.0f;
	MaxSpeedCrouched = 300.0f;
	MaxAirSpeed = 1200.0f;
	AirBrakingDeceleration = 800.0f;


	WalkAcceleration = 1024.0f;
	BrakingDecelerationWalking = WalkAcceleration;

	SetSlidableFloorZ(.31);
}

void UFPMovementComponent::PostLoad()
{
	Super::PostLoad();

	PawnOwner = Cast<AFirstPersonProjCharacter>(GetOwner());
}

void UFPMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor);

	if (CurrentFloor.IsWalkableFloor())
	{
		StartGroundMovement();
	}
}

void UFPMovementComponent::InitializeComponent()
{
	SetMovementMode(EFPMovementMode::Falling);

	if (PawnOwner)
	{
		AFirstPersonProjCharacter* FPPCharacter = Cast<AFirstPersonProjCharacter>(PawnOwner);
		check(FPPCharacter);
		const UCapsuleComponent* CharacterCapsule = FPPCharacter->GetCapsuleComponent();
		CachedDefaultCapsuleHalfHeight = CharacterCapsule->GetUnscaledCapsuleHalfHeight();
		CachedOwnerChar = FPPCharacter;

		CachedMinimumSlideSpeedSquared = StartSlideSpeedMinimum * StartSlideSpeedMinimum;
		CachedSlideSpeedThresholdSquared = SlideSpeedThreshold * SlideSpeedThreshold;
	}
}

void UFPMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PerformMovement(DeltaTime);
}

#if WITH_EDITOR
void UFPMovementComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UFPMovementComponent, WalkableFloorAngle))
	{
		// Compute WalkableFloorZ from the Angle.
		SetWalkableFloorAngle(WalkableFloorAngle);
	}
	else if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UFPMovementComponent, SlideFloorAngle))
	{
		// Compute WalkableFloorZ from the Angle.
		SetSlidableFloorAngle(SlideFloorAngle);
	}
}
#endif // WITH_EDITOR

AFirstPersonProjCharacter* UFPMovementComponent::GetFPPOwner() const
{
	return Cast<AFirstPersonProjCharacter>(GetPawnOwner());
}

float UFPMovementComponent::GetWalkableFloorAngle() const
{
	return WalkableFloorAngle;
}

void UFPMovementComponent::SetWalkableFloorAngle(float InWalkableFloorAngle)
{
	WalkableFloorAngle = InWalkableFloorAngle;
	WalkableFloorZ = FMath::Cos(FMath::DegreesToRadians(InWalkableFloorAngle));
}

float UFPMovementComponent::GetWalkableFloorZ() const
{
	return WalkableFloorZ;
}

void UFPMovementComponent::SetWalkableFloorZ(float InWalkableFloorZ)
{
	WalkableFloorZ = InWalkableFloorZ;
	WalkableFloorAngle = FMath::RadiansToDegrees(FMath::Acos(InWalkableFloorZ));
}

void UFPMovementComponent::PerformMovement(const float DeltaTime)
{
	const FVector InputVector = ConsumeInputVector();
	switch (MovementMode)
	{
		case EFPMovementMode::Falling:
			PerformFallMovement(DeltaTime, InputVector);
			break;
		case EFPMovementMode::Walking:
			PerformWalkMovement(DeltaTime, InputVector);
			break;
		case EFPMovementMode::Sliding:
			PerformSlideMovement(DeltaTime, InputVector);
			break;
		default:
			return;
	}
}

void UFPMovementComponent::PerformWalkMovement(const float DeltaTime, const FVector& InputVector)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (IsSprinting() && (!bWantsToSprint || !CanSprint(InputVector)))
	{
		SetIsSprinting(false);
	}
	else if (!IsSprinting() && bWantsToSprint && CanSprint(InputVector))
	{
		SetIsSprinting(true);
	}

	AFirstPersonProjCharacter* Character = GetFPPOwner();
	if (Character->ConsumeJumpInput() && CanJump())
	{
		DoJump();
		PerformFallMovement(DeltaTime, InputVector);
		return;
	}

	if (CanBeginSliding(CurrentFloor))
	{
		StartSliding(CurrentFloor);
		PerformSlideMovement(DeltaTime, InputVector);
		return;
	}

	TickCrouch(DeltaTime);

	float RemainingTime = DeltaTime;
	const FVector PositionBeforeMove = UpdatedComponent->GetComponentLocation();
	
	CalculateGroundVelocity(InputVector, DeltaTime);
	const FVector InitialDelta = Velocity * DeltaTime;
	FVector MoveDelta = Velocity * DeltaTime;
	// Project velocity onto floor normal to move up ramps.
	if (CurrentFloor.IsWalkableFloor() && CurrentFloor.HitResult.Normal.Z < 1.0f && IsWalkableSurface(CurrentFloor.HitResult))
	{
		float RampProjection = MoveDelta | CurrentFloor.HitResult.Normal;
		MoveDelta.Z = -RampProjection / CurrentFloor.HitResult.Normal.Z; // Why? Why are we calculating it like this.
	}

	if (MoveDelta.IsNearlyZero())
	{
		return;
	}


	FHitResult MoveHitResult(1.0f);
	SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, MoveHitResult);

	if (MoveHitResult.bStartPenetrating)
	{
		SlideAlongSurface(MoveDelta, 1.0f, MoveHitResult.ImpactNormal, MoveHitResult, true);
	}
	else if (MoveHitResult.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = MoveHitResult.Time;

		// If hit is a ramp, try moving up it.
		if (MoveHitResult.Time > 0.0f && MoveHitResult.Normal.Z > UE_KINDA_SMALL_NUMBER && IsWalkableSurface(MoveHitResult))
		{
			float RampProjection = MoveDelta | CurrentFloor.HitResult.Normal;
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			MoveDelta = InitialDelta * InitialPercentRemaining;
			MoveDelta.Z = -RampProjection / CurrentFloor.HitResult.Normal.Z;
			SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, MoveHitResult);

			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + (MoveHitResult.Time * InitialPercentRemaining), 0.0f, 1.0f);
		}

		if (MoveHitResult.IsValidBlockingHit())
		{
			if (CanStepUp(MoveHitResult))
			{
				// Try to step on top of barrier.
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				const FVector GravityDir = FVector::DownVector;
				FStepDownResult StepDownResult;
				if (!StepUp(GravityDir, MoveDelta * (1.0f - PercentTimeApplied), MoveHitResult, &StepDownResult))
				{
					SlideAlongSurface(MoveDelta, 1.0f - PercentTimeApplied, MoveHitResult.Normal, MoveHitResult, true);
				}
			}
		}
		else if (MoveHitResult.Component.IsValid() && MoveHitResult.Component.Get()->CanCharacterStepUp(PawnOwner))
		{
			SlideAlongSurface(MoveDelta, 1.0f - PercentTimeApplied, MoveHitResult.Normal, MoveHitResult, true);
		}
	}

	Velocity = (UpdatedComponent->GetComponentLocation() - PositionBeforeMove) / DeltaTime;
	Velocity.Z = 0.0f;

	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor);
	if (!CurrentFloor.IsWalkableFloor())
	{
		StartFalling();
	}

}

void UFPMovementComponent::CalculateGroundVelocity(const FVector& InputVector, float DeltaTime)
{
	if (InputVector.IsNearlyZero() && Velocity.IsNearlyZero())
	{
		return;
	}

	const float PreviousVelocity2D = Velocity.Size2D();
	const float CurrentMaxGroundSpeed = FMath::Lerp(IsSprinting() ? MaxSprintSpeed : MaxWalkSpeed, MaxSpeedCrouched, CrouchFrac);
	const FVector TargetVelocity = InputVector.GetSafeNormal2D() * CurrentMaxGroundSpeed;
	FVector AccelerationVec = TargetVelocity - Velocity;
	const bool bIsDecelerating = InputVector.IsNearlyZero() || TargetVelocity.SizeSquared2D() < (PreviousVelocity2D * PreviousVelocity2D);

	if (AccelerationVec.IsNearlyZero())
	{
		return;
	}

	float AccelerationToUse = WalkAcceleration;
	if (bIsDecelerating)
	{
		AccelerationToUse = BrakingDecelerationWalking;
	}
	else
	{
		Velocity = Velocity - (Velocity - AccelerationVec.GetSafeNormal2D() * Velocity.Size2D()) * DeltaTime;
		AccelerationVec = TargetVelocity - Velocity;
	}
	AccelerationToUse *= DeltaTime;

	// Prevent new velocity from exceeding desired velocity.
	FVector VelocityDelta = (AccelerationVec.GetSafeNormal2D() * AccelerationToUse);
	//UE_LOG(LogTemp, Warning, TEXT("before Vel: %s, Delta: %s, Accel Vec: %s"), *Velocity.ToString(), *VelocityDelta.ToString(), *AccelerationVec.ToString());
	if (VelocityDelta.SizeSquared2D() > AccelerationVec.SizeSquared2D())
	{
		VelocityDelta *= AccelerationVec.Size2D() / VelocityDelta.Size2D();
	}
	Velocity += VelocityDelta;
	//UE_LOG(LogTemp, Warning, TEXT("after Vel: %s, after Delta: %s"), *Velocity.ToString(), *VelocityDelta.ToString());
}

bool UFPMovementComponent::CanSprint(const FVector& InputVector) const
{
	if (bWantsToCrouch || !IsMovingOnGround())
	{
		return false;
	}

	const float InputActorDirectionDot = InputVector.GetSafeNormal() | PawnOwner->GetActorForwardVector();
	return InputActorDirectionDot >= .6f;
}

void UFPMovementComponent::StartGroundMovement()
{
	if (CurrentFloor.IsWalkableFloor())
	{
		if (MovementMode == EFPMovementMode::Falling)
		{
			AFirstPersonProjCharacter* FPPCharacter = GetFPPOwner();
			check(FPPCharacter);
			FPPCharacter->OnLanded(CurrentFloor.HitResult);
		}

		SetMovementMode(EFPMovementMode::Walking);

		Velocity.Z = 0.0f;
	}
}

bool UFPMovementComponent::StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& InHit, FStepDownResult* OutStepDownResult)
{
	// This function moves up, over the obstacle, then down to the floor.

	if (!CanStepUp(InHit) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	float PawnRadius, PawnHalfHeight;
	const AFirstPersonProjCharacter* FPPCharacter = Cast<AFirstPersonProjCharacter>(PawnOwner);
	if (!FPPCharacter)
	{
		return false;
	}
	FPPCharacter->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Don't bother stepping up if top of capsule is hitting something.
	const float InitialImpactZ = InHit.ImpactPoint.Z;
	if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	{
		return false;
	}

	if (GravDir.IsZero())
	{
		return false;
	}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(InHit.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;

	if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + MAX_FLOOR_DIST * 2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(InHit.Location, InHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= CurrentFloor.FloorDist;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	if (InitialImpactZ <= PawnInitialFloorBaseZ)
	{
		return false;
	}

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(-GravDir * StepTravelUpHeight, PawnRotation, true, &SweepUpHit);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	// step fwd
	FHitResult Hit(1.f);
	MoveUpdatedComponent(Delta, PawnRotation, true, &Hit);

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			// Undo movement
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(SweepUpHit);
		}

		// pawn ran into a wall
		HandleImpact(Hit);
		if (IsFalling())
		{
			return true;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);

		if (IsFalling())
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}

	// Step down
	MoveUpdatedComponent(GravDir * StepTravelDownHeight, UpdatedComponent->GetComponentQuat(), true, &Hit);

	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = Hit.ImpactPoint.Z - PawnFloorPointZ;
		if (DeltaZ > MaxStepHeight)
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, NewLocation.Z);
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!IsWalkableSurface(Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s opposed to movement)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (Hit.Location.Z > OldLocation.Z)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s above old position)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (outside edge tolerance)"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f && !CanStepUp(Hit))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (up onto surface with !CanStepUp())"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutStepDownResult != NULL)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			if (Hit.Location.Z > OldLocation.Z)
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideZ < .08f)
				{
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}

			StepDownResult.bComputedFloor = true;
		}
	}

	// Copy step down result.
	if (OutStepDownResult != NULL)
	{
		*OutStepDownResult = StepDownResult;
	}

	return true;
}

bool UFPMovementComponent::ShouldCheckForValidLandingSpot(float DeltaTime, const FHitResult& Hit) const
{
	// See if we hit an edge of a surface on the lower portion of the capsule.
	// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
	if (Hit.Normal.Z > UE_KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		const AFirstPersonProjCharacter* FPPCharacter = Cast<AFirstPersonProjCharacter>(PawnOwner);
		check(FPPCharacter);
		const UCapsuleComponent* CharacterCapsule = FPPCharacter->GetCapsuleComponent();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, CharacterCapsule->GetScaledCapsuleRadius()))
		{
			return true;
		}
	}

	return false;
}

bool UFPMovementComponent::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const
{
	const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}

bool UFPMovementComponent::IsSprinting() const
{
	return bIsSprinting;
}

void UFPMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult) const
{
	const AFirstPersonProjCharacter* FPPCharacter = Cast<AFirstPersonProjCharacter>(PawnOwner);
	check(FPPCharacter);
	const UCapsuleComponent* CharacterCapsule = FPPCharacter->GetCapsuleComponent();
	check(CharacterCapsule);
	float PawnRadius, PawnHalfHeight;
	CharacterCapsule->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);
	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;

	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(PawnRadius - CAPSULE_RADIUS_SHRINK_FACTOR, PawnHalfHeight);
	const float TraceHeight = MaxStepHeight + MAX_FLOOR_DIST;
	const FVector EndTraceLocation = CapsuleLocation + (FVector::DownVector * TraceHeight);

	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(PawnOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(CollisionQueryParams, ResponseParams);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();


	FHitResult SweepHitResult;
	GetWorld()->SweepSingleByChannel(SweepHitResult, CapsuleLocation, EndTraceLocation, CharacterCapsule->GetComponentQuat(), CharacterCapsule->GetCollisionObjectType(), CapsuleShape, CollisionQueryParams, ResponseParams);

	if (SweepHitResult.bBlockingHit)
	{
		const float SweepDistance = FMath::Max(-MAX_FLOOR_DIST, SweepHitResult.Time * TraceHeight);
		OutFloorResult.SetFromSweep(SweepHitResult, SweepDistance, false);

		if (IsWalkableSurface(SweepHitResult))
		{
			OutFloorResult.bWalkableFloor = true;
			return;
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = FMath::Max(-MAX_FLOOR_DIST, SweepHitResult.Time * TraceHeight);
		return;
	}

	// Line trace
	if (FloorLineTraceDist > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;
		const float TraceDist = FloorLineTraceDist + ShrinkHeight;
		const FVector Down = FVector(0.f, 0.f, -TraceDist);
		CollisionQueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bool bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, CollisionQueryParams, ResponseParams);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= FloorLineTraceDist && IsWalkableSurface(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}

	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
}

void UFPMovementComponent::SetIsSprinting(bool bNewIsSprinting)
{
	if (!bIsSprinting && bNewIsSprinting)
	{
		SetWantsToCrouch(false);
	}

	bIsSprinting = bNewIsSprinting;
}

bool UFPMovementComponent::IsWalkableSurface(const FHitResult& FloorHitResult) const
{
	return FloorHitResult.IsValidBlockingHit() && FloorHitResult.GetActor() != nullptr && FloorHitResult.ImpactNormal.Z >= WalkableFloorZ;
}

bool UFPMovementComponent::CanStepUp(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit() || !PawnOwner || MovementMode == MOVE_Falling)
	{
		return false;
	}

	// No component for "fake" hits when we are on a known good base.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	if (!HitComponent->CanCharacterStepUp(PawnOwner))
	{
		return false;
	}

	// No actor for "fake" hits when we are on a known good base.

	if (!Hit.HitObjectHandle.IsValid())
	{
		return true;
	}

	const AActor* HitActor = Hit.HitObjectHandle.GetManagingActor();
	if (!HitActor->CanBeBaseForCharacter(PawnOwner))
	{
		return false;
	}

	return true;
}

void UFPMovementComponent::OnGroundMovementStopped()
{
	SetIsSprinting(false);
}

float UFPMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.0f;
	}

	FVector MoveNormal = Normal;
	if (IsMovingOnGround())
	{
		if (Normal.Z > 1.0f)
		{
			if (!IsWalkableSurface(Hit))
			{
				MoveNormal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -UE_KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					MoveNormal = FloorNormal;
				}

				MoveNormal = Normal.GetSafeNormal2D();
			}
		}
	}

	return Super::SlideAlongSurface(Delta, Time, MoveNormal, Hit, bHandleImpact);
}

void UFPMovementComponent::StartFalling()
{
	if (!IsFalling())
	{
		CurrentFloor.Clear();
		TimeFallStartedSeconds = GetWorld()->GetTimeSeconds();
		InitialJumpVelocity= Velocity.GetSafeNormal2D() * Velocity.Size2D();
		SetMovementMode(EFPMovementMode::Falling);
	}
}

void UFPMovementComponent::PerformFallMovement(const float DeltaTime, const FVector& InputVector)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	FVector PositionBeforeMove = UpdatedComponent->GetComponentLocation();

	AFirstPersonProjCharacter* Character = GetFPPOwner();
	if (Character->ConsumeJumpInput() && CanJump())
	{
		DoJump();
	}

	TickCrouch(DeltaTime);

	CalculateFallVelocity(InputVector, DeltaTime);

	const FVector MoveDelta = Velocity * DeltaTime;
	FHitResult MoveHitResult(1.0f);
	SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, MoveHitResult);

	if (MoveHitResult.IsValidBlockingHit())
	{
		if (MoveHitResult.Time > 0.0f)
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - PositionBeforeMove) / (MoveHitResult.Time * DeltaTime);
		}

		//DrawDebugSphere(GetWorld(), MoveHitResult.ImpactPoint, 15.0f, 3, FColor::Green, false, 5.0f, 0, .2f);

		if (IsWalkableSurface(MoveHitResult) || ShouldCheckForValidLandingSpot(DeltaTime, MoveHitResult))
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor);

			if (CanBeginSliding(CurrentFloor))
			{
				StartSliding(CurrentFloor);
				return;
			}
			else if (CurrentFloor.IsWalkableFloor())
			{
				StartGroundMovement();
				return;
			}
		}

		PositionBeforeMove = UpdatedComponent->GetComponentLocation();
		const float PrevTimeRemaining = 1.0f - MoveHitResult.Time;
		const FVector SlideDelta = MoveDelta * (1.0f - MoveHitResult.Time);
		SlideAlongSurface(SlideDelta, 1.0f, MoveHitResult.Normal, MoveHitResult, true);

		if (MoveHitResult.Time > 0)
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - PositionBeforeMove) / ((PrevTimeRemaining * MoveHitResult.Time) * DeltaTime);
		}
	}
}

void UFPMovementComponent::CalculateFallVelocity(const FVector& InputVector, float DeltaTime)
{
	const FVector ForwardVector = UpdatedComponent->GetForwardVector();
	const FVector RightVector = UpdatedComponent->GetRightVector();
	const FVector LateralInputVector = InputVector.ProjectOnTo(RightVector);

	FVector ForwardVelocity = Velocity.ProjectOnToNormal(ForwardVector);
	FVector LateralVelocity = Velocity.ProjectOnToNormal(RightVector);

	float MaxForwardAirVelocity = FMath::Min(MaxAirSpeed, FMath::Max(InitialJumpVelocity.Size2D(), MaxAirSpeed * .20f));
	FVector TargetForwardVelocity = InputVector.IsNearlyZero() ? ForwardVelocity : InputVector.ProjectOnToNormal(ForwardVector) * MaxForwardAirVelocity;

	FVector InputLateralTargetVelocity = LateralInputVector * MaxAirStrafe;
	FVector TargetLateralVelocity = InputVector.IsNearlyZero() ? LateralVelocity : FMath::Max(InputLateralTargetVelocity.Size(), LateralVelocity.Size()) * InputLateralTargetVelocity.GetSafeNormal2D();

	//UE_LOG(LogTemp, Warning, TEXT("Forward Velocity: %s, Lateral Velocity: %s, Current Velocity: %s"), *ForwardVelocity.ToString(), *LateralVelocity.ToString(), *Velocity.ToString());
	//UE_LOG(LogTemp, Warning, TEXT("Input Vec: %s, Target Forward Velocity: %s, Target Lateral Velocity: %s"), *InputVector.GetSafeNormal2D().ToString(), *TargetForwardVelocity.ToString(), *TargetLateralVelocity.ToString());

	const FVector TargetVelocity = TargetForwardVelocity + TargetLateralVelocity + (FVector::DownVector * GetPhysicsVolume()->TerminalVelocity);
	FVector Acceleration = TargetVelocity - Velocity;

	FVector ForwardAcceleration = Acceleration.ProjectOnToNormal(ForwardVector);
	const float ForwardAccelerationDot = ForwardAcceleration.GetSafeNormal2D() | InputVector.GetSafeNormal2D();
	if (ForwardAccelerationDot <= -.1f)
	{
		ForwardAcceleration = ForwardAcceleration.GetSafeNormal2D() * AirBrakingDeceleration * -ForwardAccelerationDot;
	}
	else
	{
		// Increase acceleration if the player is providing lateral input in the direction they want to turn in the air.
		// Start by checking how orthogonal the forward vector and velocity are. The more orthogonal, the more the player has to turn.
		// Scale this value by the dot product between the initial jump vector and the input. This is to ensure the player is inputting the correct direction into the turn.
		const float TurnAccelerationScalar = (ForwardVector ^ Velocity.GetSafeNormal2D()).Size() * FMath::Max(0.0f, InitialJumpVelocity.GetSafeNormal2D() | -LateralInputVector);
		const float ForwardAirAcceleration = AirAcceleration * FMath::Lerp(1.0f, 3.0f, TurnAccelerationScalar);
		//UE_LOG(LogTemp, Warning, TEXT("Air acceleration bonus: %f, final: %f"),  AirAccelerationInputBonus, AirAcceleration + AirAccelerationInputBonus);
		ForwardAcceleration = ForwardAcceleration.GetSafeNormal2D() * ForwardAirAcceleration;
	}

	FVector LateralAcceleration = Acceleration.ProjectOnToNormal(RightVector);
	const float LateralAccelerationDot = LateralAcceleration.GetSafeNormal2D() | InputVector.GetSafeNormal2D();
	if (LateralAccelerationDot <= -.1f)
	{
		LateralAcceleration = LateralAcceleration.GetSafeNormal2D() * AirBrakingDeceleration * -LateralAccelerationDot;
	}
	else
	{
		LateralAcceleration = LateralAcceleration.GetSafeNormal2D() * AirAcceleration;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Lat acc: %s, Fow acc:%s"), *LateralAcceleration.ToString(), *ForwardAcceleration.ToString());
	
	FVector VelocityDelta = LateralAcceleration + ForwardAcceleration;

	// Subtract the deceleration vector from the velocity to allow the player to change directions.
	// Scale by friction.
	if (!Acceleration.GetSafeNormal2D().IsNearlyZero())
	{
		const FVector Velocity2D = FVector(Velocity.X, Velocity.Y, 0);
		Velocity = Velocity - (Velocity2D - VelocityDelta.GetSafeNormal2D() * Velocity2D.Size()) * DeltaTime * AirFrictionFactor;
		Acceleration = TargetVelocity - Velocity;
		//UE_LOG(LogTemp, Warning, TEXT("Old vel: %s, New Vel: %s, Accel vector: %s"), *OldVel.ToString(), *Velocity.ToString(), *Acceleration.ToString());
	}
	
	if (VelocityDelta.Size2D() > Acceleration.Size2D())
	{
		VelocityDelta *= Acceleration.Size2D() / VelocityDelta.Size2D();
	}

	VelocityDelta.Z = GetGravityZ();
	VelocityDelta *= DeltaTime;
	//UE_LOG(LogTemp, Warning, TEXT("Target Velocity: %s, Vel Delta: %s"), *TargetVelocity.ToString(),  *VelocityDelta.ToString());

	Velocity += VelocityDelta;
	Velocity.Z = FMath::Max(Velocity.Z, -GetPhysicsVolume()->TerminalVelocity);
	//UE_LOG(LogTemp, Warning, TEXT("New Velocity: %s"), *Velocity.ToString());
}

void UFPMovementComponent::PerformSlideMovement(const float DeltaTime, const FVector& InputVector)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (CachedOwnerChar->ConsumeJumpInput() && CanJump())
	{
		DoJump();
		PerformFallMovement(DeltaTime, InputVector);
		return;
	}

	TickCrouch(DeltaTime);

	float RemainingDeltaTime = DeltaTime;
	const FVector PositionBeforeSlide = UpdatedComponent->GetComponentLocation();
	int32 Iterations = 0;
	FVector GravitationalAcceleration = FVector::ZeroVector;
	bool bCouldPreviouslyWalkOnSurface = SlideFloorResult.IsWalkableFloor();

	while (RemainingDeltaTime > 0.0f && Iterations < 3)
	{
		CalculateSlideVelocity(DeltaTime, InputVector, GravitationalAcceleration);
		FVector MoveDelta = Velocity * RemainingDeltaTime;
		FHitResult SlideHitResult = SlideFloorResult.HitResult;
		SlideAlongSurface(MoveDelta, 1.0f, SlideFloorResult.HitResult.Normal, SlideHitResult, true);
		RemainingDeltaTime -= RemainingDeltaTime * SlideHitResult.Time;

		/*
		if (SlideHitResult.bBlockingHit)
		{
			bool bShouldMoveAgain = true;
			if (FMath::IsNearlyZero(SlideHitResult.Normal.Z))
			{
				const FVector Vel2D = Velocity.GetSafeNormal2D();
				const float VelocityNormalDot = -SlideHitResult.Normal | Vel2D;
				if (VelocityNormalDot <= .6)
				{
					// Try to bounce off of the surface.
					const FVector DeflectionVectorDirection2D = Vel2D - Vel2D.ProjectOnToNormal(SlideHitResult.Normal) * 2;
					const FVector NewVelocity = DeflectionVectorDirection2D * Velocity.Size2D() * (1.0f - VelocityNormalDot);
					Velocity = FVector(NewVelocity.X, NewVelocity.Y, Velocity.Z);
				}
				else
				{
					break;
				}
			}
			else
			{
				// We may have hit another flatter surface. Recheck the floor and try again.
				FindFloor(UpdatedComponent->GetComponentLocation(), SlideFloorResult);

				if (CanSlideOnSurface(SlideFloorResult))
				{
					const FVector GravityAccelerationDirection = FVector::VectorPlaneProject(FVector::DownVector, SlideFloorResult.HitResult.Normal).GetSafeNormal();
					const FVector CurrentVelDirection = Velocity.GetSafeNormal();
					const FVector NewVelDirection = FVector(CurrentVelDirection.X, CurrentVelDirection.Y, GravityAccelerationDirection.Z).GetSafeNormal();
					Velocity = Velocity.Size() * NewVelDirection;
				}
				else
				{
					break;
				}
			}
		}
		*/

		++Iterations;
	}

	FindFloor(UpdatedComponent->GetComponentLocation(), SlideFloorResult);
	if (!bCouldPreviouslyWalkOnSurface && SlideFloorResult.IsWalkableFloor())
	{
		CachedOwnerChar->OnLanded(SlideFloorResult.HitResult);
	}

	if (RemainingDeltaTime < DeltaTime)
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - PositionBeforeSlide) / (DeltaTime - RemainingDeltaTime);
	}

	bool bShouldStopSlide = !bWantsToCrouch || !CanSlideOnSurface(SlideFloorResult);
	bShouldStopSlide |= GravitationalAcceleration.IsNearlyZero(4.0f) && Velocity.SizeSquared2D() <= CachedSlideSpeedThresholdSquared;
	UE_LOG(LogTemp, Warning, TEXT("Velocity: %s, Grav acceleration: %s"), *Velocity.ToString(), *GravitationalAcceleration.ToString());
	if (bShouldStopSlide)
	{
		if (!SlideFloorResult.IsWalkableFloor())
		{
			StartFalling();
			PerformFallMovement(RemainingDeltaTime, InputVector);
		}
		else
		{
			CurrentFloor = SlideFloorResult;
			StartGroundMovement();
			PerformWalkMovement(RemainingDeltaTime, InputVector);
		}
	}
}

bool UFPMovementComponent::CanBeginSliding(const FFindFloorResult& FloorResult) const
{
	return bWantsToCrouch && Velocity.SizeSquared() >= CachedMinimumSlideSpeedSquared && CanSlideOnSurface(FloorResult);
}

bool UFPMovementComponent::CanSlideOnSurface(const FFindFloorResult& FloorResult) const
{
	return FloorResult.bBlockingHit && FloorResult.HitResult.Normal.Z >= SlideFloorZ;
}

void UFPMovementComponent::SetSlidableFloorAngle(float Angle)
{
	SlideFloorAngle = Angle;
	SlideFloorZ = FMath::Cos(FMath::DegreesToRadians(Angle));
}

void UFPMovementComponent::SetSlidableFloorZ(float InWalkableFloorZ)
{
	SlideFloorZ = InWalkableFloorZ;
	SlideFloorAngle = FMath::RadiansToDegrees(FMath::Acos(InWalkableFloorZ));
}

void UFPMovementComponent::StartSliding(const FFindFloorResult& NewSlideFloor)
{
	SlideFloorResult = NewSlideFloor;
	SetMovementMode(EFPMovementMode::Sliding);
}

void UFPMovementComponent::CalculateSlideVelocity(float DeltaTime, const FVector& InputVector, FVector& OutGravitationalAccelVec)
{
	const FVector GravityAcceelerationDirection = FVector::VectorPlaneProject(FVector::DownVector, SlideFloorResult.HitResult.Normal).GetSafeNormal();
	const float GravityAccelerationRatio = (1.0f - SlideFloorResult.HitResult.Normal.Z) / (1.0f - SlideFloorZ);
	OutGravitationalAccelVec = GravityAcceelerationDirection * SlideGravityAcceleration * GravityAccelerationRatio;

	FVector SlideFrictionAccelerationVector = FVector::ZeroVector;
	const float VelocityGravityDot = GravityAcceelerationDirection | Velocity.GetSafeNormal();
	// If we are moving perpindicular to the gravity vector, apply slide friction.
	if (FMath::Abs(VelocityGravityDot) <= .1f)
	{
		SlideFrictionAccelerationVector = -Velocity.GetSafeNormal2D() * Velocity.Size2D() * SlideFrictionFactor * (1.0f - GravityAccelerationRatio);
	}

	// Consider lateral slide input and deceleration.
	FVector InputAcceleration = FVector::ZeroVector;

	float InputVelocityDot = Velocity.GetSafeNormal2D() | InputVector.GetSafeNormal2D();
	if (InputVelocityDot <= -.45f)
	{
		InputAcceleration += Velocity.GetSafeNormal() * InputVelocityDot * SlideBrakingDeceleration;
	}
	if (!InputAcceleration.IsNearlyZero())
	{
		// Subtract the deceleration vector from the velocity to allow the player to change directions.
		// Scale by friction.
		const FVector Velocity2D = FVector(Velocity.X, Velocity.Y, 0);
		//Velocity = Velocity - (Velocity2D - InputAcceleration.GetSafeNormal() * Velocity2D.Size()) * DeltaTime * .3f;
		//UE_LOG(LogTemp, Warning, TEXT("Old vel: %s, New Vel: %s, Accel vector: %s"), *OldVel.ToString(), *Velocity.ToString(), *Acceleration.ToString());
	}

	FVector LateralVec = Velocity.GetSafeNormal2D() ^ FVector::UpVector;
	const FVector LateralInputVec = InputVector.ProjectOnToNormal(LateralVec) * SlideLateralAcceleration;
	//InputAcceleration += LateralInputVec;
	//UE_LOG(LogTemp, Warning, TEXT("Projection: %s, Lateral Vector: %s"), *InputVector.ProjectOnToNormal(LateralVec).ToString(), *LateralInputVec.ToString());

	//UE_LOG(LogTemp, Warning, TEXT("Slide: Ratio: %f, Grav accel: %s, friction: %s, input: %s"), GravityAccelerationRatio, *OutGravitationalAccelVec.ToString(), *SlideFrictionAccelerationVector.ToString(), *InputAcceleration.ToString());
	FVector FinalAcceleration = (OutGravitationalAccelVec + SlideFrictionAccelerationVector + InputAcceleration) * DeltaTime;

	Velocity += FinalAcceleration;
}

bool UFPMovementComponent::IsSliding() const
{
	return MovementMode == EFPMovementMode::Sliding;
}

void UFPMovementComponent::SetMovementMode(EFPMovementMode NewMovementMode)
{
	const EFPMovementMode OldMode = MovementMode;
	MovementMode = NewMovementMode;
	OnMovementModeChanged(OldMode, MovementMode);
}

float UFPMovementComponent::GetCrouchFrac() const
{
	return CrouchFrac;
}

void UFPMovementComponent::OnMovementModeChanged(EFPMovementMode OldMovementMode, EFPMovementMode NewMovementMode)
{
	switch (OldMovementMode)
	{
		case EFPMovementMode::Walking:
			OnGroundMovementStopped();
			break;
		default:
			break;
	}
}

bool UFPMovementComponent::CanCharacterUncrouch() const
{
	if (!IsCrouching())
	{
		return true;
	}

	const UCapsuleComponent* CharacterCapsule = CachedOwnerChar->GetCapsuleComponent();
	check(CharacterCapsule);
	float PawnRadius, PawnHalfHeight;
	CharacterCapsule->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
	FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(PawnRadius - CAPSULE_RADIUS_SHRINK_FACTOR, CapsuleCrouchHalfHeight);

	const float HalfHeightDifference = CachedDefaultCapsuleHalfHeight - CapsuleCrouchHalfHeight;
	const FVector UncrouchPosition = UpdatedComponent->GetComponentLocation() + (FVector::UpVector * HalfHeightDifference);

	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(PawnOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(CollisionQueryParams, ResponseParams);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	FHitResult HitResult(1.0f);
	GetWorld()->SweepSingleByChannel(HitResult, UpdatedComponent->GetComponentLocation(), UncrouchPosition, UpdatedComponent->GetComponentQuat(), CollisionChannel, CapsuleShape, CollisionQueryParams, ResponseParams);

	if (HitResult.bBlockingHit)
	{
		DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 20.0f, 4, FColor::Red, false, 5.0f, 0, .5f);
	}
	return !HitResult.bBlockingHit;
}

float UFPMovementComponent::GetCrouchedHalfHeight() const
{
	return CapsuleCrouchHalfHeight;
}

float UFPMovementComponent::GetDefaultCapsuelHalfHeight() const
{
	return CachedDefaultCapsuleHalfHeight;
}

bool UFPMovementComponent::IsFalling() const
{
	return UpdatedComponent && MovementMode == EFPMovementMode::Falling;
}

bool UFPMovementComponent::IsMovingOnGround() const
{
	return UpdatedComponent && MovementMode == EFPMovementMode::Walking || MovementMode == EFPMovementMode::Sliding;
}

float UFPMovementComponent::GetGravityZ() const
{
	return Super::GetGravityZ() * GravityScale;
}

const FFindFloorResult& UFPMovementComponent::GetCurrentFloorResult() const
{
	return CurrentFloor;
}

void UFPMovementComponent::SetWantsToSprint(bool WantstoSprint)
{
	bWantsToSprint = WantstoSprint;
}

bool UFPMovementComponent::CanJump()
{
	const AFirstPersonProjCharacter* FPPCharacter = GetFPPOwner();
	check(FPPCharacter);

	if (MovementMode == EFPMovementMode::Falling && (GetWorld()->GetTimeSeconds() - TimeFallStartedSeconds) > JumpGracePeriod)
	{
		return false;
	}
	if (MovementMode == EFPMovementMode::Walking && IsCrouching())
	{
		return false;
	}

	return FPPCharacter->CanCharacterJump();
}

void UFPMovementComponent::DoJump()
{
	AFirstPersonProjCharacter* FPPCharacter = GetFPPOwner();
	check(FPPCharacter);

	Velocity.Z = JumpZVelocity;

	if (MovementMode != EFPMovementMode::Falling)
	{
		StartFalling();
	}

	FPPCharacter->OnJumped();
}

void UFPMovementComponent::OnFallMovementStopped()
{
	InitialJumpVelocity = FVector::ZeroVector;
	TimeFallStartedSeconds = 0.0f;
}

bool UFPMovementComponent::CanCrouch() const
{
	return !bIsSprinting;
}

void UFPMovementComponent::SetWantsToCrouch(bool WantsToCrouch)
{
	bWantsToCrouch = WantsToCrouch;
}

void UFPMovementComponent::TickCrouch(float DeltaTime)
{
	AFirstPersonProjCharacter* FPPCharacter = GetFPPOwner();
	check(FPPCharacter);

	if (bWantsToCrouch && CrouchFrac < 1.0f && CanCrouch())
	{
		const bool bWasPreviouslyUncrouched = CrouchFrac < .5f;
		CrouchFrac = FMath::Min(CrouchFrac + (DeltaTime / (IsSliding() ? TimeToCrouchSliding : TimeToCrouchSeconds)), 1.0f);
		if (bWasPreviouslyUncrouched && CrouchFrac >= .5f)
		{
			FPPCharacter->GetCapsuleComponent()->SetCapsuleHalfHeight(CapsuleCrouchHalfHeight);
			FPPCharacter->OnCrouchChanged(true);

			if (IsMovingOnGround())
			{
				const float CrouchCapsuleDelta = CachedDefaultCapsuleHalfHeight - CapsuleCrouchHalfHeight;
				const FVector NewPosition = UpdatedComponent->GetComponentLocation() + (FVector::DownVector * CrouchCapsuleDelta);
				UpdatedComponent->SetWorldLocation(NewPosition);
			}
		}

		FPPCharacter->RecalculateBaseEyeHeight();
	
	}
	else if (!bWantsToCrouch && CrouchFrac > 0.0f)
	{
		if (CanCharacterUncrouch())
		{
			const bool bWasPreviouslyCrouched = CrouchFrac >= .5f;
			CrouchFrac = FMath::Max(CrouchFrac - (DeltaTime / TimeToCrouchSeconds), 0.0f);

			if (bWasPreviouslyCrouched && CrouchFrac < .5f)
			{
				FPPCharacter->GetCapsuleComponent()->SetCapsuleHalfHeight(CachedDefaultCapsuleHalfHeight);
				FPPCharacter->OnCrouchChanged(false);

				if (IsMovingOnGround())
				{
					const float CrouchCapsuleDelta = CachedDefaultCapsuleHalfHeight - CapsuleCrouchHalfHeight;
					const FVector NewPosition = UpdatedComponent->GetComponentLocation() + (FVector::UpVector * CrouchCapsuleDelta);
					UpdatedComponent->SetWorldLocation(NewPosition);
				}
			}

			FPPCharacter->RecalculateBaseEyeHeight();
		}
	}
}

bool UFPMovementComponent::IsCrouching() const
{
	return CrouchFrac > 0.5f;
}
