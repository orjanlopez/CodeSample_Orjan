// Fill out your copyright notice in the Description page of Project Settings.


#include "GamePawn.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "Planet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values
AGamePawn::AGamePawn()
{
 	// Init tick
	PrimaryActorTick.bCanEverTick = true;
	SetTickGroup(ETickingGroup::TG_PostUpdateWork);

	// Init components
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->SetSimulatePhysics(true);
	Capsule->SetHiddenInGame(true);
	Capsule->SetEnableGravity(false);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECollisionChannel::ECC_GameTraceChannel2, ECollisionResponse::ECR_Ignore);
	Capsule->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
	Capsule->SetUseCCD(true);
	Capsule->SetAngularDamping(30.f);
	Capsule->SetLinearDamping(0.1f);
	Capsule->SetGenerateOverlapEvents(true);
	Capsule->SetCapsuleHalfHeight(70.f);
	Capsule->SetCapsuleRadius(17.5);
	
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Capsule);
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	CharacterBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterBody"));
	CharacterBody->SetupAttachment(Capsule);
	CharacterBody->SetRenderCustomDepth(true);

	CharacterHead = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterHead"));
	CharacterHead->SetupAttachment(CharacterBody, FName("neck"));
	CharacterHead->SetRenderCustomDepth(true);
	
	CharacterFace = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CharacterFace"));
	CharacterFace->SetupAttachment(CharacterHead);

	VisorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisorMesh"));
	VisorMesh->SetupAttachment(CharacterBody, FName("neck"));
	VisorMesh->SetRenderCustomDepth(true);
	
	Hair = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hair"));
	Hair->SetupAttachment(CharacterBody, FName("neck"));
	Hair->SetRenderCustomDepth(true);

	// Init variables
	StabilizingStrength = 1000000.f;
	MovementStrength = 1000.f;
	OnGroundLinearDamping = 100.f;
	JumpStrength = 1200.f;
	AirControlMultiplier = 0.01f;
	MaxVelocity = 600.f;
	bIsFalling = false;
	LookSensitivity = 100.f;
}

// Called when the game starts or when spawned
void AGamePawn::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AGamePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Store delta time locally in class
	ActorDeltaTime = DeltaTime;

	// Tick functions
	HandleIsFalling();
	HandleCapsuleRotation();
	HandleMovement();
	HandleSKRot();
	ClampMovementVelocity();
	UpdateCurrentSpeed();
	UpdateMovementVelocity();
}

// Called to bind functionality to input
void AGamePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AGamePawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGamePawn::MoveRight);
	PlayerInputComponent->BindAxis("LookUp", this, &AGamePawn::LookUp);
	PlayerInputComponent->BindAxis("LookRight", this, &AGamePawn::LookRight);

	PlayerInputComponent->BindAction("Jump", EInputEvent::IE_Pressed, this, &AGamePawn::Jump);
}

// Movement inputs 
void AGamePawn::MoveForward(float Value)
{
	WalkYValue = Value;
}

void AGamePawn::MoveRight(float Value)
{
	WalkXValue = Value;
}

void AGamePawn::Jump()
{
	if (!bIsFalling && !bDisabledMovement)
	{
		Capsule->AddImpulse(GetActorUpVector() * JumpStrength);
	}
}

// Look inputs
void AGamePawn::LookUp(float Value)
{
	if (Value != 0.f)
	{
		if (CurrentPlayState == EPS_Spectating)
		{
			// AssignedPlayCamera->UpdateSpringArmPitch(Value);
		}
		else
		{
			LookRotation = LookRotation + (FRotator(Value * LookSensitivity * ActorDeltaTime, 0.f, 0.f));

			// Clamp pitch before rot
			float ClampedPitch = FMath::Clamp(LookRotation.Pitch, -89.f, 89.f);
			LookRotation = FRotator(ClampedPitch, LookRotation.Yaw, LookRotation.Roll);

			// Update rot
			UpdateSpringArmRotation();
		}
	}
}

void AGamePawn::LookRight(float Value)
{
	if (Value != 0.f)
	{
		if (CurrentPlayState == EPS_Spectating)
		{
			// AssignedPlayCamera->UpdateSpringArmYaw(Value);
		}

		else if (CurrentPlayState != EPlayState::EPS_Charging && CurrentPlayState != EPS_PlayingPuttingAnim)
		{
			LookRotation = LookRotation + (FRotator(0.f, Value * LookSensitivity * ActorDeltaTime, 0.f));
			UpdateSpringArmRotation();
		}
	}
}

// Called from look inputs
void AGamePawn::UpdateSpringArmRotation()
{
	SpringArm->SetRelativeRotation(LookRotation);
}

// Called from tick
// Adds torque to keep the player capsule upright based on gravity of dominant planet
void AGamePawn::HandleCapsuleRotation()
{
	DominantPlanet = GetDominantPlanet();
	if (bPlanetGravityInRange && Capsule && DominantPlanet)
	{
		FVector UpVector = GetActorUpVector();
		UpVector = UKismetMathLibrary::NegateVector(UpVector);

		FVector GravityNormalNormalized = GetActorLocation() - DominantPlanet->GetActorLocation();
		GravityNormalNormalized.Normalize();
		if (GravityNormalNormalized != FVector(0.f))
		{
			const FVector CrossedNormal = UKismetMathLibrary::Cross_VectorVector(GravityNormalNormalized, UpVector);
			Capsule->AddTorqueInRadians(CrossedNormal * StabilizingStrength * 400.f, "NAME_None", true);
		}
	}
}

// Called from HandleCapsuleRotation
APlanet* AGamePawn::GetDominantPlanet()
{
	float StrongestGravitySize = 0.f;
	APlanet* LocalDominantPlanet = nullptr;

	// Loop through all planets to find the one with the strongest gravitational pull
	for (int i = 0; i < PlanetGravities.Num(); ++i)
	{
		if (PlanetGravities[i].GravityForce.Size() > StrongestGravitySize)
		{
			StrongestGravitySize = PlanetGravities[i].GravityForce.Size();
			LocalDominantPlanet = PlanetGravities[i].Planet;
		}
	}

	if (LocalDominantPlanet)
	{
		return LocalDominantPlanet;
	}
	return nullptr;	
}

// Called from tick
// Handles both on ground and air movement
void AGamePawn::HandleMovement()
{
	if (!bDisabledMovement && Capsule)
	{
		// Handle input values
		const float XAbs = UKismetMathLibrary::Abs(WalkXValue);
		const float YAbs = UKismetMathLibrary::Abs(WalkYValue);
		const float Value = XAbs + YAbs;

		const float ValueClamped = FMath::Clamp(Value, 0.f, 1.f);

		if (ValueClamped > 0.1)
		{
			FVector CombinedWalkValue = FVector(WalkYValue, WalkXValue, 0.f);
			CombinedWalkValue.Normalize();

			if (bIsFalling)
			{
				const FRotator CapsuleRotation = Camera->GetComponentRotation();
				FVector CombinedWalkValueRotated = CapsuleRotation.RotateVector(CombinedWalkValue);
				CombinedWalkValueRotated.Normalize();
				MovementDirectionProjected = UKismetMathLibrary::ProjectVectorOnToPlane(CombinedWalkValueRotated, Capsule->GetUpVector());
				MovementDirectionProjected.Normalize();

				Capsule->AddForceAtLocation(MovementDirectionProjected * MovementStrength * AirControlMultiplier * ValueClamped, Capsule->GetComponentLocation() - (Capsule->GetUpVector() * 70.f));
			}
			else
			{
				const FRotator CameraRotation = Camera->GetComponentRotation();
				FVector CombinedWalkValueRotated = CameraRotation.RotateVector(CombinedWalkValue);
				CombinedWalkValueRotated.Normalize();
				MovementDirectionProjected = UKismetMathLibrary::ProjectVectorOnToPlane(CombinedWalkValueRotated, GroundNormal);
				MovementDirectionProjected.Normalize();

				Capsule->AddForceAtLocation(MovementDirectionProjected * MovementStrength * ValueClamped, Capsule->GetComponentLocation() + (Capsule->GetUpVector() * 1 * 1.f));
			}
		}
	}
}

// Called from tick
// Check if player's feet are touching the ground or another actor
void AGamePawn::HandleIsFalling()
{
		const TArray<AActor*> ActorsToIgnore;
		const ETraceTypeQuery TraceTypeQuery = UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_GameTraceChannel1);
		FHitResult OutHit;
		UKismetSystemLibrary::SphereTraceSingle(this, Capsule->GetComponentLocation(), Capsule->GetComponentLocation() - (Capsule->GetUpVector() * 53.f), 17.4, TraceTypeQuery, false, ActorsToIgnore, EDrawDebugTrace::None, OutHit, true, FLinearColor::Green, FLinearColor::Red, 1.f);

		if (OutHit.bBlockingHit)
		{
			bIsFalling = false;
			GroundNormal = OutHit.Normal;
			
			//Check if we should execute OnIsFallingUpdated
			if (bIsFalling)
			{
				bIsFalling = false;
				OnIsFallingUpdated();
			}
		}
		else
		{
			bIsFalling = true;
			
			//Check if we should execute OnIsFallingUpdated
			if (!bIsFalling)
			{
				bIsFalling = true;
				OnIsFallingUpdated();
			}
		}	
}

// Called from UpdateIsFalling
void AGamePawn::OnIsFallingUpdated()
{
	if (bIsFalling)
	{
		Capsule->SetLinearDamping(0.1);
	}
	else
	{
		Capsule->SetLinearDamping(OnGroundLinearDamping);
	}
}

// Called from tick
void AGamePawn::ClampMovementVelocity()
{
	const FVector CurrentVelocity = Capsule->GetComponentVelocity();
	const FVector CurrentVelocityClamped = CurrentVelocity.GetClampedToMaxSize(MaxVelocity);
	Capsule->SetPhysicsLinearVelocity(CurrentVelocityClamped);
	UKismetSystemLibrary::PrintString(this, CurrentVelocityClamped.ToString(), true, true, FLinearColor::Gray, 0.f);
	UKismetSystemLibrary::PrintString(this, FString::SanitizeFloat(MaxVelocity), true, true, FLinearColor::Gray, 0.f);
}

// Called from planets.cpp when player enters atmosphere 
void AGamePawn::UpdatePlanetGravities(APlanet* Planet, FVector GravityForce)
{
	FPlanetGravities InboundPlanet;
	InboundPlanet.Planet = Planet;
	InboundPlanet.GravityForce = GravityForce;
	bool bPlanetFound = false;
	const int32 PlanetGravitiesLen = PlanetGravities.Num();

	// Check if incoming planet already exists in the PlanetGravities array
	for (int32 i = 0; i < PlanetGravitiesLen; ++i)
	{
		if (PlanetGravities[i].Planet == InboundPlanet.Planet)
		{
			bPlanetFound = true;
			PlanetGravities[i] = InboundPlanet;
			break;
		}
	}

	// Add planet to array if not found
	if (!bPlanetFound)
	{
		PlanetGravities.Add(InboundPlanet);
	}
	
	bPlanetGravityInRange = PlanetGravitiesLen < 0;
}

// Called from tick
// Calculate local SK rotations based on forward direction of velocity
void AGamePawn::HandleSKRot()
{
	if (CharacterBody && DominantPlanet)
	{
		// Calculate new SK yaw based on movement velocity
		float NewYaw;
		if (MovementDirectionProjected != FVector(0.f) && CurrentSpeed > 10.f)
		{
			FVector GravityNormalNormalized = GetActorLocation() - DominantPlanet->GetActorLocation();
			GravityNormalNormalized.Normalize();
			FVector LocalMovementForward = UKismetMathLibrary::ProjectVectorOnToPlane(MovementDirectionProjected, GravityNormalNormalized);
			LocalMovementForward.Normalize();
			LocalMovementForward = LocalMovementForward.RotateAngleAxis(-90.f, GravityNormalNormalized);

			const FRotator LocalBodyRotation = CharacterBody->GetComponentRotation();
			LocalMovementForward = LocalBodyRotation.UnrotateVector(LocalMovementForward);
			const FVector LocalBodyForwardDirection = LocalBodyRotation.UnrotateVector(CharacterBody->GetForwardVector());
			const float RotValue = LocalMovementForward.Rotation().Yaw - LocalBodyForwardDirection.Rotation().Yaw;

			NewYaw = RotValue * 7.f * ActorDeltaTime;
		}
		else
		{
			NewYaw = 0.f;
		}

		// Calculate new roll based on movement velocity
		FVector LocalVector = CharacterBody->GetForwardVector();
		FRotator LocalRot = UKismetMathLibrary::MakeRotFromZ(LocalVector);
		LocalVector = LocalRot.UnrotateVector(LocalVector);
		
		const float NewRoll = UKismetMathLibrary::MapRangeClamped(LocalRot.Roll, -250, 250, 17.5, -17.5);
		const float RollMultiplier = UKismetMathLibrary::MapRangeClamped(MovementVelocity.Size(), 0.f, MaxVelocity, 0.f, 1.f);

		// Finally, apply rotations
		CharacterBody->AddLocalRotation(FRotator(0.f, NewYaw, 0.f));
		CharacterBody->SetRelativeRotation(FRotator(0.f, CharacterBody->GetRelativeRotation().Yaw, NewRoll * RollMultiplier));
	}
}

// Update various variables used for anims
void AGamePawn::UpdateCurrentSpeed()
{
	if (Capsule)
	{
		CurrentSpeed = Capsule->GetComponentVelocity().Size();
	}
}

void AGamePawn::UpdateZVelocity()
{
	if (bIsFalling)
	{
		const FVector CapsuleUnrotatedVelocity = Capsule->GetComponentRotation().UnrotateVector(Capsule->GetComponentVelocity());
		ZVelocity = CapsuleUnrotatedVelocity.Z;
	}
}

void AGamePawn::UpdateMovementVelocity()
{
	MovementVelocity = Capsule->GetComponentRotation().UnrotateVector(Capsule->GetComponentVelocity());
}

// Getters
UCapsuleComponent* AGamePawn::GetCapsuleComponent() const
{
	return Capsule;
}