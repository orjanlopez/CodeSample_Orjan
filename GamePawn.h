// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GamePawn.generated.h"

// Forward declarations
class UCapsuleComponent;
class USpringArmComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class APlanet;
class APlayCamera;

USTRUCT(BlueprintType)
struct FPlanetGravities
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	APlanet* Planet;
	UPROPERTY(BlueprintReadOnly)
	FVector GravityForce;
};

UENUM(BlueprintType)
enum EPlayState
{
	EPS_Regular UMETA(DisplayName = "Regular"),
	EPS_Aiming UMETA(DisplayName = "Putting"),
	EPS_Charging UMETA(DisplayName = "Charging"),
	EPS_PlayingPuttingAnim UMETA(DisplayName = "PlayingPuttingAnim"),
	EPS_Spectating UMETA(DisplayName = "Spectating")
};

UCLASS()
class SPOLF_API AGamePawn : public APawn
{
	GENERATED_BODY()

public:
	//Functions
	//Default
	AGamePawn();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	//Inputs
	UFUNCTION()
    void MoveForward(float Value);
	UFUNCTION()
    void MoveRight(float Value);
	UFUNCTION()
    void LookUp(float Value);
	UFUNCTION()
    void LookRight(float Value);
	UFUNCTION()
    void Jump();

	//Misc
	UCapsuleComponent* GetCapsuleComponent() const;
	void UpdatePlanetGravities(APlanet* Planet, FVector GravityForce);

	//Variables
	//Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components)
	class UCapsuleComponent* Capsule;

	UPROPERTY(VisibleAnywhere, Category=Components)
	class USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, Category=Components)
	class UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Components)
	class USkeletalMeshComponent* CharacterBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Components)
	class USkeletalMeshComponent* CharacterHead;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Components)
	class UStaticMeshComponent* CharacterFace;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Components)
	class UStaticMeshComponent* VisorMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Components)
	class UStaticMeshComponent* Hair;

	//Movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float StabilizingStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float MovementStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float OnGroundLinearDamping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float JumpStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float AirControlMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement)
	float MaxVelocity;

	UPROPERTY(BlueprintReadOnly, Category = Movement)
	float CurrentSpeed;

	UPROPERTY(BlueprintReadOnly, Category = Movement)
	bool bIsFalling;

	UPROPERTY(BlueprintReadOnly, Category = Movement)
	float ZVelocity;

	UPROPERTY(BlueprintReadOnly, Category = Movement)
	FVector MovementVelocity;

	bool bDisabledMovement;

	//Player
	UPROPERTY(BlueprintReadOnly, Category = Player)
	TEnumAsByte<EPlayState> CurrentPlayState;

	FRotator LookRotation;
	FVector GroundNormal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Player)
	float LookSensitivity;
	
	//Gravity
	UPROPERTY(BlueprintReadWrite, Category = Gravity)
	TArray<FPlanetGravities> PlanetGravities;
	FVector DominantGravity;

	UPROPERTY(BlueprintReadOnly, Category = Debug)
	APlanet* DominantPlanet;
	
protected:
	//Functions
	virtual void BeginPlay() override;
	APlanet* GetDominantPlanet();
	void HandleCapsuleRotation();
	void HandleMovement();
	void HandleIsFalling();
	void OnIsFallingUpdated();
	void ClampMovementVelocity();

	//Variables
	//Movement
	UPROPERTY(BlueprintReadOnly, Category = Movement)
	FVector CombinedWalkValue;

	UPROPERTY(BlueprintReadOnly, Category = Movement)
	FVector MovementDirectionProjected;

	float WalkYValue;
	float WalkXValue;

	//Misc
	bool bPlanetGravityInRange;

private:
	//Functions
	void HandleSKRot();
	void UpdateZVelocity();
	void UpdateCurrentSpeed();
	void UpdateSpringArmRotation();
	void UpdateMovementVelocity();

	//Variables
	float ActorDeltaTime;
	FVector CurrentMovementForward;
	FRotator CurrentMovementRotation;

};
