#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PPortal.generated.h"

class UArrowComponent;

UCLASS()
class PORTAL_API APPortal : public AActor
{
	GENERATED_BODY()

public:
	APPortal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

	void AuthSetLinkedPortal(APPortal* NewLinkedPortal);
	void AuthSetEmptyMaterial(UMaterial* NewEmptyMaterial);

	UFUNCTION(BlueprintCallable)
	USceneCaptureComponent2D* GetSceneCaptureComponent() const { return SceneCaptureComponent; }

	UFUNCTION(BlueprintCallable)
	UArrowComponent* GetEntrance() const { return Entrance; }

protected:
	UPROPERTY(EditDefaultsOnly)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditDefaultsOnly)
	USceneCaptureComponent2D* SceneCaptureComponent;

	UPROPERTY(EditDefaultsOnly)
	UArrowComponent* Entrance;

	UPROPERTY(ReplicatedUsing=OnRep_LinkedPortal)
	APPortal* LinkedPortal;

	UPROPERTY(ReplicatedUsing=OnRep_EmptyMaterial)
	UMaterial* EmptyMaterial;

	virtual void BeginPlay() override;

	void SetupDynamicTarget();
	void UpdateMaterial() const;
	void UpdateCamera();

private:
	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	UFUNCTION()
	void OnRep_LinkedPortal();

	UFUNCTION()
	void OnRep_EmptyMaterial() { UpdateMaterial(); }
};
