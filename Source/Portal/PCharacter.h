#pragma once

#include "CoreMinimal.h"
#include "PPortalGunComponent.h"
#include "GameFramework/Character.h"
#include "PCharacter.generated.h"

class UPPortalGunComponent;
class UPInputActions;
struct FInputActionValue;
class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UAnimMontage;
class UInputMappingContext;

DECLARE_DELEGATE_OneParam(FCrouchDelegate, bool);

UCLASS()
class APCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit APCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	UFUNCTION(BlueprintPure)
	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure)
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }

	UFUNCTION(BlueprintPure)
	UPPortalGunComponent* GetPortalGunComponent() const { return PortalGunComponent; }

protected:
	UPROPERTY(EditDefaultsOnly)
	UInputMappingContext* InputMapping;

	UPROPERTY(EditDefaultsOnly)
	UPInputActions* InputActions;

	UPROPERTY(EditDefaultsOnly)
	UCameraComponent* Camera;

	UPROPERTY(EditDefaultsOnly)
	USkeletalMeshComponent* ArmsMesh;

	UPROPERTY(EditDefaultsOnly)
	UPPortalGunComponent* PortalGunComponent;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual bool CanJumpInternal_Implementation() const override;

	void Look(const FInputActionValue& Value);
	void Move(const FInputActionValue& Value);
	void Fire(const FInputActionValue& Value);

private:
	float TargetCameraHeight;
};
