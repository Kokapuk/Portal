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

UCLASS()
class APCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit APCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Restart() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	void UpdateFirstPersonCapture(const TArray<UPrimitiveComponent*>& AdditionalComponentsToCapture);

	UFUNCTION(BlueprintCallable)
	void InitializeFirstPersonRenderTarget();

	UCameraComponent* GetCamera() const { return Camera; }
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }
	UPPortalGunComponent* GetPortalGunComponent() const { return PortalGunComponent; }

	UFUNCTION(BlueprintCallable)
	UMaterialInstanceDynamic* GetDynamicFirstPersonCaptureMaterial() const { return DynamicFirstPersonCaptureMaterial; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UCameraComponent* Camera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USkeletalMeshComponent* ArmsMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UPPortalGunComponent* PortalGunComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USceneCaptureComponent2D* FirstPersonCapture;

	UPROPERTY(EditDefaultsOnly)
	UInputMappingContext* InputMapping;

	UPROPERTY(EditDefaultsOnly)
	UPInputActions* InputActions;

	UPROPERTY(EditDefaultsOnly)
	UMaterial* FirstPersonCaptureMaterial;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual bool CanJumpInternal_Implementation() const override;

	void Look(const FInputActionValue& Value);
	void Move(const FInputActionValue& Value);
	void Fire(const FInputActionValue& Value);

private:
	float TargetCameraHeight;

	UPROPERTY()
	UTextureRenderTarget2D* FirstPersonRenderTarget;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicFirstPersonCaptureMaterial;

	void OnViewportResize(FViewport* ViewPort, uint32 Value) { InitializeFirstPersonRenderTarget(); }
};
