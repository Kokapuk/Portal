#include "PPlayerController.h"

APPlayerController::APPlayerController()
{
	RollRecoverSpeed = 5.f;
}

void APPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	RecoverRoll(DeltaSeconds);
}

void APPlayerController::BeginPlay()
{
	Super::BeginPlay();

	PlayerCameraManager->ViewRollMax = 179.f;
	PlayerCameraManager->ViewRollMin = -179.f;
}

void APPlayerController::RecoverRoll(float DeltaSeconds)
{
	FQuat CurrentRotation = ControlRotation.Quaternion();
	FQuat TargetRotation = FRotator(ControlRotation.Pitch, ControlRotation.Yaw, 0.f).Quaternion();
	FQuat NewRotation = FQuat::Slerp(CurrentRotation, TargetRotation, DeltaSeconds * RollRecoverSpeed);

	SetControlRotation(NewRotation.Rotator());
}
