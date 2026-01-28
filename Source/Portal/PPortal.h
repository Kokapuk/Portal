#pragma once

#include "CoreMinimal.h"

#include "Components/RectLightComponent.h"

#include "GameFramework/Actor.h"

#include "PPortal.generated.h"

class URectLightComponent;
class APPortal;
class UBoxComponent;
class UArrowComponent;
class USoundCue;

struct FPostUpdateWorkTickFunction : FTickFunction
{
	APPortal* Target;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
	                         const FGraphEventRef& MyCompletionGraphEvent) override;
};

UCLASS()
class PORTAL_API APPortal : public AActor
{
	GENERATED_BODY()

public:
	APPortal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(NetMulticast, Reliable)
	void MultiInitialize(const FLinearColor& DefaultColor);

	void AuthSetLinkedPortal(APPortal* NewLinkedPortal);
	void AuthSetSurface(AActor* NewSurface);

	USceneCaptureComponent2D* GetSceneCaptureComponent() const { return SceneCaptureComponent; }
	UArrowComponent* GetEntrance() const { return Entrance; }
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; };

	UFUNCTION(BlueprintPure)
	AActor* GetSurface() const { return Surface; }

	void UpdateCamera();

protected:
	UPROPERTY(EditDefaultsOnly, meta=(Units="Centimeters"))
	float Height;

	UPROPERTY(EditDefaultsOnly, meta=(Units="Centimeters"))
	float Width;

	UPROPERTY(EditDefaultsOnly, meta=(Units="Centimeters"))
	float FrameThickness;

	UPROPERTY(EditDefaultsOnly, meta=(Units="Centimeters"))
	float FrameLength;

	UPROPERTY(EditDefaultsOnly, meta=(Units="Centimeters"))
	float TriggerLength;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin=0.f, ClampMax=1.f))
	float TeleportationThreshold;

	UPROPERTY(EditDefaultsOnly)
	USoundCue* EnterSound;

	FLinearColor Color;

	UPROPERTY(Replicated)
	AActor* Surface;

	UPROPERTY(ReplicatedUsing=OnRep_LinkedPortal)
	APPortal* LinkedPortal;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneCaptureComponent2D* SceneCaptureComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UArrowComponent* Entrance;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* Trigger;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameTop;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameRight;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameBottom;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* FrameLeft;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	URectLightComponent* Light;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	void InitializeRenderTarget();
	void UpdateMaterial() const;
	void UpdateTrigger() const;
	void UpdateLight() const { Light->SetLightColor(Color); }
	void CheckTransition();

private:
	FPostUpdateWorkTickFunction PostUpdateWorkTick;
	const float BaseMeshSize = 100.f;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                           int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	                         int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_LinkedPortal();
};
