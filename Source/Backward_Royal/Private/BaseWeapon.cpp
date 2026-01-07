#include "BaseWeapon.h"
#include "Kismet/GameplayStatics.h"

// 로그 매크로
DEFINE_LOG_CATEGORY_STATIC(LogBaseWeapon, Display, All);

#define LOG_WEAPON(Verbosity, Format, ...) \
    UE_LOG(LogBaseWeapon, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(TEXT(Format), ##__VA_ARGS__))

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;

    // 1. 메시 생성 및 루트 설정
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    // 2. 충돌 설정 (매우 중요)
    // BoxComponent 때와 똑같은 설정을 메시에 직접 적용합니다.
    WeaponMesh->SetNotifyRigidBodyCollision(true); // Simulation Generates Hit Events
    WeaponMesh->SetCollisionProfileName(TEXT("Custom")); // 커스텀 설정 사용

    // 초기 상태: 물리 충돌은 꺼두거나(NoCollision) 혹은 들고 다니는 상태에 맞춤
    // 공격 전에는 보통 캐릭터 캡슐과 겹치지 않게 NoCollision 혹은 IgnoreOnlyPawn 등으로 둡니다.
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    WeaponMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);

    // 공격 시 켜질 때를 대비한 기본 반응 (Pawn과 PhysicsBody는 막음)
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_PhysicsBody, ECollisionResponse::ECR_Block);
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();

    // 메시 자체의 Hit 이벤트 바인딩
    if (WeaponMesh)
    {
        WeaponMesh->OnComponentHit.AddDynamic(this, &ABaseWeapon::OnWeaponHit);
    }
}

void ABaseWeapon::StartAttack()
{
    LOG_WEAPON(Display, "StartAttack: Mesh Physics Blocking Enabled");

    // 메시의 충돌을 켭니다.
    if (WeaponMesh)
    {
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

void ABaseWeapon::StopAttack()
{
    LOG_WEAPON(Display, "StopAttack: Mesh Collision Disabled");

    // 메시의 충돌을 끕니다.
    if (WeaponMesh)
    {
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ABaseWeapon::OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this && OtherActor != GetOwner())
    {
        // 1. 충격량 기반 데미지 계산
        float ImpactForce = NormalImpulse.Size();
        float DamageCoefficient = 0.01f; // 조절 필요
        float CalculatedDamage = ImpactForce * DamageCoefficient;

        // 최소 데미지 필터링
        if (CalculatedDamage < 5.0f) return;

        // 최대 데미지 제한
        CalculatedDamage = FMath::Min(CalculatedDamage, 100.0f);

        LOG_WEAPON(Warning, "MESH HIT! Force: %.2f -> Damage: %.2f", ImpactForce, CalculatedDamage);

        // 2. 데미지 적용
        UGameplayStatics::ApplyDamage(
            OtherActor,
            CalculatedDamage,
            GetInstigatorController(),
            this,
            UDamageType::StaticClass()
        );

        // 3. 물리 반동 (Half Sword 스타일)
        if (OtherComp && OtherComp->IsSimulatingPhysics())
        {
            FVector HitDirection = -Hit.ImpactNormal;
            float PushMultiplier = 0.5f;

            OtherComp->AddImpulseAtLocation(HitDirection * ImpactForce * PushMultiplier, Hit.ImpactPoint);
        }
    }
}