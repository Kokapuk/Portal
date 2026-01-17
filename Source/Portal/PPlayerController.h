#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PPlayerController.generated.h"

UCLASS()
class PORTAL_API APPlayerController : public APlayerController
{
	GENERATED_BODY()

	virtual void AddYawInput(float Value) override { Super::AddYawInput(Value * MouseSensitivity); }
	virtual void AddPitchInput(float Value) override { Super::AddPitchInput(Value * MouseSensitivity); }

protected:
	UFUNCTION(BlueprintCallable)
	void SetMouseSensitivity(float Value) { MouseSensitivity = Value; };

private:
	float MouseSensitivity;
};
