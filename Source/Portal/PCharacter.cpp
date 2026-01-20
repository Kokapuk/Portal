#include "PCharacter.h"

#include "PCharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "PInputActions.h"
#include "PPortalGunComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

APCharacter::APCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPCharacterMovementComponent>(CharacterMovementComponentName))
{
	BaseEyeHeight = 166.f;
	CrouchedEyeHeight = 115.f;
	TargetCameraHeight = BaseEyeHeight;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCapsuleHalfHeight(80.f);
	Capsule->SetCapsuleRadius(30.f);

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	CharacterMesh->SetRelativeRotation(FRotator(0.f, -Capsule->GetScaledCapsuleHalfHeight(), 0.f));
	CharacterMesh->SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetScaledCapsuleHalfHeight()));
	CharacterMesh->SetRelativeScale3D(FVector::OneVector * 0.9f);
	CharacterMesh->SetOwnerNoSee(true);
	CharacterMesh->SetGenerateOverlapEvents(false);
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(CharacterMesh);
	Camera->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	Camera->SetFieldOfView(103.f);
	Camera->bUsePawnControlRotation = true;

	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>("ArmsMesh");
	ArmsMesh->SetRelativeLocation(FVector(0.f, 10.f, -160.f));
	ArmsMesh->SetupAttachment(Camera);

	FirstPersonCapture = CreateDefaultSubobject<USceneCaptureComponent2D>("FirstPersonCapture");
	FirstPersonCapture->SetupAttachment(Camera);
	FirstPersonCapture->FOVAngle = Camera->FieldOfView;
	FirstPersonCapture->bCaptureOnMovement = false;
	FirstPersonCapture->bCaptureEveryFrame = false;

	PortalGunComponent = CreateDefaultSubobject<UPPortalGunComponent>("PortalGunComponent");
}

void APCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
}

void APCharacter::BeginPlay()
{
	Super::BeginPlay();

	GEngine->GameViewport->Viewport->ViewportResizedEvent.AddUObject(this, &APCharacter::OnViewportResize);
}

void APCharacter::Restart()
{
	Super::Restart();
	UpdateFirstPersonCapture({});
}

void APCharacter::Tick(float DeltaSeconds)
{
	const FVector NewCameraLocation = UKismetMathLibrary::VInterpTo(Camera->GetRelativeLocation(),
	                                                                FVector(0.f, 0.f, TargetCameraHeight), DeltaSeconds,
	                                                                10.f);
	Camera->SetRelativeLocation(NewCameraLocation);

	Super::Tick(DeltaSeconds);
}

void APCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	TargetCameraHeight = CrouchedEyeHeight;
}

void APCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	TargetCameraHeight = BaseEyeHeight;
}

void APCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(IsValid(InputMapping))
	check(IsValid(InputActions))
	check(IsValid(InputActions->LookAction))
	check(IsValid(InputActions->MoveAction))
	check(IsValid(InputActions->JumpAction))
	check(IsValid(InputActions->CrouchAction))
	check(IsValid(InputActions->FireAction))

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
		PlayerController->GetLocalPlayer());

	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMapping, 0);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	Input->BindAction(InputActions->LookAction, ETriggerEvent::Triggered, this, &APCharacter::Look);

	Input->BindAction(InputActions->MoveAction, ETriggerEvent::Triggered, this, &APCharacter::Move);

	Input->BindAction(InputActions->JumpAction, ETriggerEvent::Started, this, &APCharacter::Jump);
	Input->BindAction(InputActions->JumpAction, ETriggerEvent::Completed, this, &APCharacter::StopJumping);

	Input->BindAction(InputActions->CrouchAction, ETriggerEvent::Started, this, &APCharacter::Crouch, false);
	Input->BindAction(InputActions->CrouchAction, ETriggerEvent::Completed, this, &APCharacter::UnCrouch, false);

	Input->BindAction(InputActions->FireAction, ETriggerEvent::Started, this, &APCharacter::Fire);
}

bool APCharacter::CanJumpInternal_Implementation() const
{
	return JumpIsAllowedInternal();
}

void APCharacter::InitializeFirstPersonRenderTarget()
{
	check(IsValid(FirstPersonCaptureMaterial))

	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);

	FirstPersonRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		this, ViewportSize.X, ViewportSize.Y, RTF_RGBA16f);

	FirstPersonCapture->TextureTarget = FirstPersonRenderTarget;

	DynamicFirstPersonCaptureMaterial = UMaterialInstanceDynamic::Create(FirstPersonCaptureMaterial, this);
	DynamicFirstPersonCaptureMaterial->SetTextureParameterValue("Texture", FirstPersonRenderTarget);
}

void APCharacter::UpdateFirstPersonCapture(const TArray<UPrimitiveComponent*>& AdditionalComponentsToCapture)
{
	const bool LocallyControlled = IsLocallyControlled();
	FirstPersonCapture->bCaptureEveryFrame = LocallyControlled;

	TArray<UPrimitiveComponent*> ComponentsToCapture;
	ComponentsToCapture.Add(ArmsMesh);
	ComponentsToCapture.Append(AdditionalComponentsToCapture);

	FirstPersonCapture->ClearShowOnlyComponents();
	for (UPrimitiveComponent* Component : ComponentsToCapture)
	{
		Component->SetVisibility(LocallyControlled);
		Component->SetVisibleInSceneCaptureOnly(true);
		if (LocallyControlled) FirstPersonCapture->ShowOnlyComponent(Component);
	}
}

void APCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D AxisValue = Value.Get<FVector2D>();

	AddControllerYawInput(AxisValue.X);
	AddControllerPitchInput(-AxisValue.Y);
}

void APCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D AxisValue = Value.Get<FVector2D>();

	AddMovementInput(GetActorForwardVector(), AxisValue.X);
	AddMovementInput(GetActorRightVector(), AxisValue.Y);
}

void APCharacter::Fire(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();

	if (AxisValue > 0) PortalGunComponent->CosmeticFire(0);
	else PortalGunComponent->CosmeticFire(1);
}
