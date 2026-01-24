#pragma once

#include "CoreMinimal.h"

#include "GameFramework/PlayerController.h"

#include "PPlayerController.generated.h"

UCLASS()
class PORTAL_API APPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APPlayerController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void AddYawInput(float Value) override { Super::AddYawInput(Value * MouseSensitivity); }
	virtual void AddPitchInput(float Value) override { Super::AddPitchInput(Value * MouseSensitivity); }

protected:
	UPROPERTY(EditDefaultsOnly)
	float RollRecoverSpeed;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void SetMouseSensitivity(float Value) { MouseSensitivity = Value; };

	void RecoverRoll(float DeltaSeconds);

private:
	float MouseSensitivity;
};
