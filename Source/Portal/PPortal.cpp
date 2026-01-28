#include "PPortal.h"

#include "Types.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"

#include "Engine/TextureRenderTarget2D.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "Net/UnrealNetwork.h"

#include "Sound/SoundCue.h"

APPortal::APPortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	PostUpdateWorkTick.bCanEverTick = true;
	PostUpdateWorkTick.TickGroup = TG_PostUpdateWork;
	PostUpdateWorkTick.Target = this;

	bReplicates = true;
	SetReplicateMovement(true);

	Height = 215.f;
	Width = 115.f;
	FrameThickness = 40.f;
	FrameLength = 60.f;
	TriggerLength = 60.f;
	TeleportationThreshold = 0.002f; // NearClipPlane + 0.001 as safe buffer zone

	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>("USceneCaptureComponent");
	SceneCaptureComponent->SetupAttachment(RootComponent);
	SceneCaptureComponent->FOVAngle = 103.f;
	SceneCaptureComponent->bEnableClipPlane = true;
	SceneCaptureComponent->bAlwaysPersistRenderingState = true;
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;

	Entrance = CreateDefaultSubobject<UArrowComponent>("Entrance");
	Entrance->SetupAttachment(RootComponent);
	Entrance->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	Entrance->SetHiddenInSceneCapture(true);

	Trigger = CreateDefaultSubobject<UBoxComponent>("Trigger");
	Trigger->SetupAttachment(RootComponent);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->ShapeColor = FColor::Green;

	FrameTop = CreateDefaultSubobject<UBoxComponent>("FrameTop");
	FrameRight = CreateDefaultSubobject<UBoxComponent>("FrameRight");
	FrameBottom = CreateDefaultSubobject<UBoxComponent>("FrameBottom");
	FrameLeft = CreateDefaultSubobject<UBoxComponent>("FrameLeft");

	for (TArray Frames = {FrameTop, FrameRight, FrameBottom, FrameLeft}; UBoxComponent* Frame : Frames)
	{
		Frame->SetupAttachment(RootComponent);
		Frame->SetCollisionProfileName("BlockAllDynamic");
		Frame->SetCollisionResponseToChannel(ECC_PORTAL, ECR_Ignore);
		Frame->ShapeColor = FColor::Red;
	}

	Light = CreateDefaultSubobject<URectLightComponent>("Light");
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(50.f);
	Light->SetAttenuationRadius(200.f);
}

void APPortal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APPortal, Surface);
	DOREPLIFETIME(APPortal, LinkedPortal);
}

void APPortal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const float HalfHeight = Height / 2.f;
	const float HalfWidth = Width / 2.f;
	const float HalfFrameLength = FrameLength / 2.f;
	const float HalfFrameThickness = FrameThickness / 2.f;
	const float HalfTriggerLength = TriggerLength / 2.f;

	Mesh->SetRelativeScale3D(FVector(1.f, Width / BaseMeshSize, Height / BaseMeshSize));
	Mesh->SetRelativeLocation(FVector::ZeroVector);

	FrameTop->SetBoxExtent(FVector(HalfFrameLength, HalfWidth, HalfFrameThickness));
	FrameTop->SetRelativeLocation(FVector(-HalfFrameLength, 0.f, HalfHeight + HalfFrameThickness));

	FrameRight->SetBoxExtent(FVector(HalfFrameLength, HalfFrameThickness, HalfHeight));
	FrameRight->SetRelativeLocation(FVector(-HalfFrameLength, -(HalfWidth + HalfFrameThickness), 0.f));

	FrameBottom->SetBoxExtent(FVector(HalfFrameLength, HalfWidth, HalfFrameThickness));
	FrameBottom->SetRelativeLocation(FVector(-HalfFrameLength, 0.f, -(HalfHeight + HalfFrameThickness)));

	FrameLeft->SetBoxExtent(FVector(HalfFrameLength, HalfFrameThickness, HalfHeight));
	FrameLeft->SetRelativeLocation(FVector(-HalfFrameLength, HalfWidth + HalfFrameThickness, 0.f));

	Trigger->SetBoxExtent(FVector(HalfTriggerLength, HalfWidth, HalfHeight));
	Trigger->SetRelativeLocation(FVector::ZeroVector);

	Light->SetSourceHeight(Height);
	Light->SetSourceWidth(Width);
	Light->SetRelativeLocation(FVector::ZeroVector);
}

void APPortal::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		PostUpdateWorkTick.RegisterTickFunction(GetLevel());
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APPortal::OnTriggerBeginOverlap);
	Trigger->OnComponentEndOverlap.AddDynamic(this, &APPortal::OnTriggerEndOverlap);
}

void APPortal::InitializeRenderTarget()
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		this, ViewportSize.X, ViewportSize.Y, RTF_RGBA16f);
	SceneCaptureComponent->TextureTarget = RenderTarget;
}

void APPortal::UpdateMaterial() const
{
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;

	check(IsValid(RenderTarget));

	UMaterialInstanceDynamic* DynamicMaterial = Mesh->CreateDynamicMaterialInstance(0);

	if (IsValid(LinkedPortal))
	{
		DynamicMaterial->SetVectorParameterValue("Color", Color);
		DynamicMaterial->SetTextureParameterValue("View", LinkedPortal->GetRenderTarget());
		DynamicMaterial->SetScalarParameterValue("ViewOpacity", 1.f);
	}
	else
	{
		DynamicMaterial->SetVectorParameterValue("Color", Color);
		DynamicMaterial->SetScalarParameterValue("ViewOpacity", 0.f);
	}
}

void APPortal::UpdateTrigger() const
{
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, IsValid(LinkedPortal) ? ECR_Overlap : ECR_Ignore);
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

void APPortal::CheckTransition()
{
	check(IsValid(EnterSound))

	if (!IsValid(LinkedPortal)) return;

	TArray<AActor*> ActorsInTrigger;
	Trigger->GetOverlappingActors(ActorsInTrigger);

	for (AActor* Actor : ActorsInTrigger)
	{
		ACharacter* Character = Cast<ACharacter>(Actor);

		if (/*!HasAuthority() && */!Character->IsLocallyControlled()) continue;

		FVector RelativePosition = GetTransform().InverseTransformPosition(Character->GetActorLocation());
		FVector RelativeVelocity = Entrance->GetComponentTransform().InverseTransformVector(
			Character->GetVelocity());
		FVector PotentialPositionNextFrame = RelativePosition + -RelativeVelocity * GetWorld()->GetDeltaSeconds();

		if (PotentialPositionNextFrame.X <= TeleportationThreshold)
		{
			FVector NewLocation(0.1f, -RelativePosition.Y, RelativePosition.Z);
			NewLocation = LinkedPortal->GetTransform().TransformPosition(NewLocation);

			FQuat RelativeControlRotation = Entrance->GetComponentTransform().InverseTransformRotation(
				Character->GetControlRotation().Quaternion());
			FRotator NewControlRotation = LinkedPortal->GetTransform().TransformRotation(RelativeControlRotation).
			                                            Rotator();

			FVector NewVelocity = LinkedPortal->GetTransform().TransformVector(RelativeVelocity);

			if (Character->IsLocallyControlled())
			{
				Character->GetController()->SetControlRotation(NewControlRotation);
			}

			Character->GetMovementComponent()->Velocity = FVector::ZeroVector;
			Character->SetActorLocation(NewLocation);
			Character->GetMovementComponent()->Velocity = NewVelocity;
			Character->GetCharacterMovement()->UpdateComponentVelocity();

			UGameplayStatics::PlaySound2D(this, EnterSound);
		}

		break;
	}
}

void APPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                     UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                     const FHitResult& SweepResult)
{
	check(IsValid(Surface));

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Surface, true);
	}
}

void APPortal::OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	check(IsValid(Surface));

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Surface, false);
	}
}

void APPortal::OnRep_LinkedPortal()
{
	UpdateMaterial();
	UpdateTrigger();
}

void APPortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckTransition();
}

void APPortal::MultiInitialize_Implementation(const FLinearColor& DefaultColor)
{
	InitializeRenderTarget();
	Color = DefaultColor;
	UpdateMaterial();
	UpdateLight();
}

void FPostUpdateWorkTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
                                              const FGraphEventRef& MyCompletionGraphEvent)
{
	Target->UpdateCamera();
	Target->GetSceneCaptureComponent()->CaptureScene();
}

void APPortal::AuthSetSurface(AActor* NewSurface)
{
	if (!HasAuthority()) return;

	Surface = NewSurface;
}

void APPortal::AuthSetLinkedPortal(APPortal* NewLinkedPortal)
{
	if (!HasAuthority()) return;

	LinkedPortal = NewLinkedPortal;
	OnRep_LinkedPortal();
}
