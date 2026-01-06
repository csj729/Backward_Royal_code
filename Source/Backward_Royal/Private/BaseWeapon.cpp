// BaseWeapon.cpp
#include "BaseWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

// 로그 카테고리 정의
DEFINE_LOG_CATEGORY(LogBaseWeapon);

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = true;

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    // 물리 트레이스를 직접 수행하므로, 메쉬 자체의 충돌 이벤트는 비활성화하거나 QueryOnly로 설정
    WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 기본값 설정
    TraceChannel = ECC_Pawn; // 캐릭터를 감지하도록 설정
    bShowDebugTrace = false;
    bIsAttacking = false;

    // 기본 스탯 초기화 (나중에 CSV/JSON에서 덮어씌워짐)
    WeaponStats.BaseDamage = 10.0f;
    WeaponStats.MassKg = 1.5f; // 예: 1.5kg 한손검
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();

    WEAPON_LOG(Display, TEXT("Weapon Initialized. Mass: %f kg"), WeaponStats.MassKg);

    // 소켓 초기 위치 캐싱
    for (const FName& SocketName : TraceSocketNames)
    {
        if (WeaponMesh->DoesSocketExist(SocketName))
        {
            PreviousSocketLocations.Add(SocketName, WeaponMesh->GetSocketLocation(SocketName));
        }
        else
        {
            WEAPON_LOG(Warning, TEXT("Socket '%s' not found on WeaponMesh!"), *SocketName.ToString());
        }
    }
}

void ABaseWeapon::StartAttack()
{
    bIsAttacking = true;
    HitActors.Empty(); // 타격 목록 초기화

    // 공격 시작 시점의 위치로 갱신 (공격 안 할 때 텔레포트 등으로 위치가 튀는 것 방지)
    for (const FName& SocketName : TraceSocketNames)
    {
        if (WeaponMesh->DoesSocketExist(SocketName))
        {
            PreviousSocketLocations.Add(SocketName, WeaponMesh->GetSocketLocation(SocketName));
        }
    }

    WEAPON_LOG(Log, TEXT("Attack Trace Started"));
}

void ABaseWeapon::EndAttack()
{
    bIsAttacking = false;
    WEAPON_LOG(Log, TEXT("Attack Trace Ended"));
}

void ABaseWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 공격 중일 때만 물리 트레이스 및 데미지 판정 수행
    if (bIsAttacking)
    {
        PerformWeaponTrace();
    }
    else
    {
        // 공격 중이 아닐 때도 위치는 계속 추적해야 다음 공격 시작 시 궤적이 튀지 않음
        for (const FName& SocketName : TraceSocketNames)
        {
            if (WeaponMesh->DoesSocketExist(SocketName))
            {
                PreviousSocketLocations.Add(SocketName, WeaponMesh->GetSocketLocation(SocketName));
            }
        }
    }
}

void ABaseWeapon::PerformWeaponTrace()
{
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    QueryParams.AddIgnoredActor(GetOwner());
    QueryParams.bTraceComplex = true;
    QueryParams.bReturnPhysicalMaterial = true;

    float DeltaTime = GetWorld()->GetDeltaSeconds();

    for (const FName& SocketName : TraceSocketNames)
    {
        if (!WeaponMesh->DoesSocketExist(SocketName)) continue;

        // [변수 정의 위치] 여기서 StartLoc과 EndLoc이 생성됩니다.
        FVector StartLoc = PreviousSocketLocations[SocketName];
        FVector EndLoc = WeaponMesh->GetSocketLocation(SocketName);

        // 1. 소켓별 이동 거리 및 속도 계산
        float DistanceMoved = FVector::Dist(StartLoc, EndLoc);
        float ImpactSpeed = (DeltaTime > 0.f) ? (DistanceMoved / DeltaTime) : 0.f;

        // 2. 레이캐스트 수행
        FHitResult HitResult;
        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            StartLoc,
            EndLoc,
            TraceChannel,
            QueryParams
        );

        // 디버그 드로잉
        if (bShowDebugTrace)
        {
            DrawDebugLine(GetWorld(), StartLoc, EndLoc, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 1.0f);
        }

        // 3. 충돌 처리
        if (bHit && HitResult.GetActor())
        {
            if (!HitActors.Contains(HitResult.GetActor()))
            {
                HitActors.Add(HitResult.GetActor());

                // 데미지 계산
                float FinalDamage = CalculatePhysicsDamage(HitResult, SocketName, ImpactSpeed);

                // 타격 방향 벡터 (시작점 -> 끝점)
                FVector AttackDirection = (EndLoc - StartLoc).GetSafeNormal();

                // 데미지 적용
                UGameplayStatics::ApplyPointDamage(
                    HitResult.GetActor(),
                    FinalDamage,
                    AttackDirection,
                    HitResult,
                    GetInstigatorController(),
                    this,
                    UDamageType::StaticClass()
                );

                // ==========================================================
                // [추가된 넉백 로직] StartLoc, EndLoc이 유효한 이 블록 안에 작성해야 합니다.
                // ==========================================================

                // 충격량 계산: (무기 질량 * 타격 속도) * 조절계수
                float KnockbackForce = (WeaponStats.MassKg * ImpactSpeed) * 0.5f;

                // 일정 힘 이상일 때만 밀려남
                if (KnockbackForce > 100.0f)
                {
                    ACharacter* TargetCharacter = Cast<ACharacter>(HitResult.GetActor());
                    if (TargetCharacter)
                    {
                        // 캐릭터를 띄우면서 밀어내기 위한 벡터 계산
                        FVector LaunchVelocity = AttackDirection * KnockbackForce;

                        // 바닥 마찰을 무시하기 위해 Z축으로 살짝 띄워줍니다 (최소 100~200)
                        LaunchVelocity.Z = FMath::Max(LaunchVelocity.Z, 150.0f);

                        // 캐릭터 밀쳐내기 실행 (LaunchCharacter는 CharacterMovementComponent 호환 함수)
                        TargetCharacter->LaunchCharacter(LaunchVelocity, false, false);

                        WEAPON_LOG(Log, TEXT("Knockback Applied! Force: %f"), KnockbackForce);
                    }
                    // 캐릭터가 아니라 물체(박스 등)인 경우 물리 시뮬레이션 적용
                    else if (HitResult.GetComponent() && HitResult.GetComponent()->IsSimulatingPhysics())
                    {
                        // 물체는 훨씬 더 큰 힘이 필요하므로 계수를 높임
                        HitResult.GetComponent()->AddImpulseAtLocation(AttackDirection * KnockbackForce * 50.0f, HitResult.ImpactPoint);
                    }
                }
                // ==========================================================
            }
        }

        // 현재 위치 저장 (다음 프레임용)
        PreviousSocketLocations.Add(SocketName, EndLoc);
    }
}

float ABaseWeapon::CalculatePhysicsDamage(const FHitResult& HitResult, FName HitSocketName, float ImpactSpeed)
{
    // --- [물리 연산 파트] ---
    // 안쪽 소켓은 ImpactSpeed가 낮아 데미지가 자연스럽게 낮게 나옵니다.
    float SpeedFactor = FMath::Clamp(ImpactSpeed / 1000.0f, 0.0f, 3.0f); // 1000cm/s를 기준속도로 가정

    // F = ma 개념 응용: 질량 * 속도 팩터 * 상수(밸런싱용)
    float PhysicsDamage = WeaponStats.MassKg * SpeedFactor * 10.0f;

    // 1. 기본 데미지와 물리 데미지 합산
    float RawDamage = WeaponStats.BaseDamage + PhysicsDamage;

    WEAPON_LOG(Verbose, TEXT("Socket: %s, Speed: %f, PhysicsDmg: %f"), *HitSocketName.ToString(), ImpactSpeed, PhysicsDamage);

    // --- [타입 및 재질 보정 파트] ---
    float Multiplier = 1.0f;

    // A. 방어구 착용 여부 판별 (Tag 혹은 Interface 사용)
    // 예시: 태그로 'Armored'를 가진 적에게는 도검류가 약함
    bool bIsArmored = HitResult.GetActor()->ActorHasTag(TEXT("Armored"));

    if (WeaponStats.DamageCategory == EDamageCategory::Slash_Pierce)
    {
        // 도검류: 방어구에 50% 데미지, 맨몸에 100~120% 데미지
        Multiplier = bIsArmored ? 0.5f : WeaponStats.FleshDamageMultiplier;
    }
    else if (WeaponStats.DamageCategory == EDamageCategory::Blunt)
    {
        // 둔기류: 방어구 관통 효율이 좋음 (예: 120%), 맨몸엔 평범 (100%)
        Multiplier = bIsArmored ? 1.2f : 1.0f;
    }

    // B. 약점 부위(Head) 판별
    // Skeletal Mesh의 Bone Name을 확인하여 헤드샷 판정
    if (HitResult.BoneName == TEXT("Head"))
    {
        Multiplier *= 2.0f; // 헤드샷 2배
        WEAPON_LOG(Warning, TEXT("CRITICAL HIT! Headshot on %s"), *HitResult.GetActor()->GetName());
    }

    // 최종 데미지 산출
    float FinalDamage = RawDamage * Multiplier;

    WEAPON_LOG(Log, TEXT("Hit Actor: %s | Part: %s | FinalDmg: %f (Base: %.1f + Phys: %.1f) x Mod: %.1f"),
        *HitResult.GetActor()->GetName(),
        *HitResult.BoneName.ToString(),
        FinalDamage,
        WeaponStats.BaseDamage,
        PhysicsDamage,
        Multiplier
    );

    return FinalDamage;
}