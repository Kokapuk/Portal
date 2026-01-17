#include "PCharacterMovementComponent.h"

UPCharacterMovementComponent::UPCharacterMovementComponent()
{
	bCanWalkOffLedgesWhenCrouching = true;
	NavAgentProps.bCanCrouch = true;
	MaxWalkSpeed = 540.f;
	MaxWalkSpeedCrouched = 240.f;
	AirControl = 1.f;
	SetCrouchedHalfHeight(60.f);
	PerchRadiusThreshold = 10.f;
}

bool UPCharacterMovementComponent::CanAttemptJump() const
{
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}
