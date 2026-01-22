#include "StaminaComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UStaminaComponent::UStaminaComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicated(true);

    MaxStamina = 100.0f;
    CurrentStamina = MaxStamina;
}

void UStaminaComponent::BeginPlay()
{
    Super::BeginPlay();
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

void UStaminaComponent::OnRep_IsSprinting()
{
    // 캐릭터에게 "상태가 변했으니 속도를 조절해라"라고 알림
    OnSprintStateChanged.Broadcast(bIsSprinting);
}

// StaminaComponent.cpp

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        // 변경 전 값 저장
        float OldStamina = CurrentStamina;
        bool bActuallyMoving = GetOwner()->GetVelocity().SizeSquared() > 10.0f;

        // --- 스태미나 계산 로직 (이전 코드 유지) ---
        if (bIsSprinting && bActuallyMoving)
        {
            CurrentStamina -= StaminaDrainRate * DeltaTime;
            if (CurrentStamina <= 0.0f)
            {
                CurrentStamina = 0.0f;
                if (bIsSprinting)
                {
                    bIsSprinting = false;
                    OnRep_IsSprinting(); // [참고] 스프린트 상태 변경도 서버는 수동 호출 필요
                }
            }
        }
        else
        {
            if (CurrentStamina < MaxStamina)
            {
                CurrentStamina += StaminaRegenRate * DeltaTime;
                if (CurrentStamina > MaxStamina) CurrentStamina = MaxStamina;
            }
        }
        // ----------------------------------------

        // [중요] 값이 조금이라도 변했다면 UI 업데이트 (서버 전용)
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