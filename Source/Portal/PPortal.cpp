#include "PPortal.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Net/UnrealNetwork.h"

APPortal::APPortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	
	SetReplicates(true);

	RootComponent = Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");

	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>("USceneCaptureComponent");
	SceneCaptureComponent->SetupAttachment(RootComponent);
	SceneCaptureComponent->FOVAngle = 103.f;
	SceneCaptureComponent->bEnableClipPlane = true;
	SceneCaptureComponent->bAlwaysPersistRenderingState = true;

	Entrance = CreateDefaultSubobject<UArrowComponent>("Entrance");
	Entrance->SetupAttachment(RootComponent);
	Entrance->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Entrance->SetHiddenInSceneCapture(true);
}

void APPortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APPortal, LinkedPortal);
}

void APPortal::BeginPlay()
{
	Super::BeginPlay();
	SetupDynamicTarget();
}

void APPortal::SetupDynamicTarget()
{
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);

	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	RenderTarget->InitCustomFormat(ViewportSize.X, ViewportSize.Y, PF_FloatRGBA, true);
	
	RenderTarget->ClearColor = FLinearColor::Black;
}

void APPortal::UpdateMaterial() const
{
	check(IsValid(RenderTarget));

	if (IsValid(LinkedPortal))
	{
		UMaterialInstanceDynamic* DynMaterial = Mesh->CreateDynamicMaterialInstance(0);

		DynMaterial->SetTextureParameterValue("PortalTexture", RenderTarget);
		Mesh->SetMaterial(0, DynMaterial);
	}
	else Mesh->SetMaterial(0, EmptyMaterial);
}

void APPortal::UpdateCamera()
{
	const APlayerController* LocalController = GetWorld()->GetFirstPlayerController();
	if (!IsValid(LinkedPortal) || !IsValid(LocalController)) return;

	APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager;
	UArrowComponent* LinkedPortalEntrance = LinkedPortal->GetEntrance();
	check((IsValid(CameraManager)))
	check((IsValid(LinkedPortalEntrance)));

	FVector CameraRelativePos = LinkedPortalEntrance->GetComponentTransform().
	                                                  InverseTransformPosition(CameraManager->GetCameraLocation());
	FRotator CameraRelativeRotation = LinkedPortalEntrance->GetComponentTransform().InverseTransformRotation(
		CameraManager->GetCameraRotation().Quaternion()).Rotator();

	SceneCaptureComponent->
		SetRelativeLocation(FVector(CameraRelativePos.X, -CameraRelativePos.Y, -CameraRelativePos.Z));
	SceneCaptureComponent->SetRelativeLocation(CameraRelativePos);
	SceneCaptureComponent->SetRelativeRotation(CameraRelativeRotation);

	FVector PlaneLocation = GetActorLocation();
	FVector PlaneNormal = GetActorForwardVector();

	SceneCaptureComponent->ClipPlaneNormal = PlaneNormal + (FVector::ForwardVector * PlaneNormal);
	SceneCaptureComponent->ClipPlaneBase = PlaneLocation;
}

void APPortal::OnRep_LinkedPortal()
{
	if (IsValid(LinkedPortal))
	{
		USceneCaptureComponent2D* LinkedPortalSceneCaptureComponent = LinkedPortal->GetSceneCaptureComponent();
		check(IsValid(LinkedPortalSceneCaptureComponent));

		LinkedPortalSceneCaptureComponent->TextureTarget = RenderTarget;
	}

	UpdateMaterial();
}

void APPortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateCamera();
}

void APPortal::AuthSetLinkedPortal(APPortal* NewLinkedPortal)
{
	if (!HasAuthority()) return;

	LinkedPortal = NewLinkedPortal;
	OnRep_LinkedPortal();
}

void APPortal::AuthSetEmptyMaterial(UMaterial* NewEmptyMaterial)
{
	if (!HasAuthority()) return;

	EmptyMaterial = NewEmptyMaterial;
	OnRep_EmptyMaterial();
}
