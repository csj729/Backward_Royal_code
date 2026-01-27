#include "StaminaComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"

// [신규] 정적 변수 초기화 (기본값 설정)
float UStaminaComponent::Global_SprintDrainRate = 20.0f;
float UStaminaComponent::Global_JumpCost = 15.0f;
float UStaminaComponent::Global_RegenRate = 10.0f;

UStaminaComponent::UStaminaComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);

    MaxStamina = 100.0f;
    CurrentStamina = MaxStamina;
}

void UStaminaComponent::BeginPlay()
{
    Super::BeginPlay();

    StaminaDrainRate = Global_SprintDrainRate;
    JumpCost = Global_JumpCost;
    StaminaRegenRate = Global_RegenRate;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        CurrentStamina = MaxStamina;
    }
    OnRep_CurrentStamina();
}

void UStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UStaminaComponent, CurrentStamina);
    DOREPLIFETIME(UStaminaComponent, bIsSprinting);
}

void UStaminaComponent::ServerSetSprinting_Implementation(bool bNewSprinting)
{
    // 상태가 변했을 때만 처리
    if (bIsSprinting != bNewSprinting)
    {
        bIsSprinting = bNewSprinting;

        // 서버는 OnRep이 자동 호출되지 않으므로 수동 호출하여 로직 실행
        OnRep_IsSprinting();
    }
}

void UStaminaComponent::ConsumeJumpStamina()
{
    // 권한(서버) 확인은 호출하는 쪽(Character)에서 하거나 여기서 한 번 더 체크
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (CurrentStamina >= JumpCost)
        {
            CurrentStamina = FMath::Clamp(CurrentStamina - JumpCost, 0.0f, MaxStamina);
            OnRep_CurrentStamina();
        }
    }
}

void UStaminaComponent::OnRep_IsSprinting()
{
    // 캐릭터에게 "상태가 변했으니 속도를 조절해라"라고 알림
    OnSprintStateChanged.Broadcast(bIsSprinting);
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        float OldStamina = CurrentStamina;
        bool bActuallyMoving = GetOwner()->GetVelocity().SizeSquared() > 10.0f;

        // 공중 상태 확인
        bool bIsFalling = false;
        if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
        {
            if (UCharacterMovementComponent* MoveComp = OwnerChar->GetCharacterMovement())
            {
                bIsFalling = MoveComp->IsFalling();
            }
        }

        // [수정] 공중에서 bIsSprinting을 false로 강제하는 로직 삭제.
        // 대신 아래 조건문에서 (!bIsFalling)을 체크하여 공중에서는 스태미나가 닳지 않게만 처리합니다.

        // 달리기 상태이고 + 실제로 움직이고 있으며 + 바닥에 있을 때만 소모
        if (bIsSprinting && bActuallyMoving && !bIsFalling)
        {
            CurrentStamina -= StaminaDrainRate * DeltaTime;
            if (CurrentStamina <= 0.0f)
            {
                CurrentStamina = 0.0f;
                if (bIsSprinting)
                {
                    bIsSprinting = false;
                    OnRep_IsSprinting();
                }
            }
        }
        else
        {
            // 스태미나 회복
            // (공중이거나, 멈춰있거나, 걷는 중이면 회복)
            if (CurrentStamina < MaxStamina)
            {
                CurrentStamina += StaminaRegenRate * DeltaTime;
                if (CurrentStamina > MaxStamina) CurrentStamina = MaxStamina;
            }
        }

        if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina))
        {
            OnRep_CurrentStamina();
        }
    }
}

void UStaminaComponent::OnRep_CurrentStamina()
{
    OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

float UStaminaComponent::GetStaminaRatio() const
{
    return (MaxStamina > 0.f) ? (CurrentStamina / MaxStamina) : 0.f;
}