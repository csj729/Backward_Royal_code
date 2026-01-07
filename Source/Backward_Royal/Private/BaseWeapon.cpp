#include "BaseWeapon.h"
#include "BaseCharacter.h"
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
    WeaponMesh->SetSimulatePhysics(true);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);    

    WeaponMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);

    // 공격 시 켜질 때를 대비한 기본 반응 (Pawn과 PhysicsBody는 막음)
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Block);
    WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    
    WeaponMesh->SetUseCCD(true);
    WeaponMesh->bTraceComplexOnMove = true;

    bIsEquipped = false;
    GripSocketName = TEXT("Grip");

    DamageCoefficient = 1.0f;
    ImpulseCoefficient = 1.0f;
    AttackSpeedCoefficient = 1.0f;
}

void ABaseWeapon::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // 에디터에서 스태틱 메시 에셋 변경 시 즉시 반영
    if (WeaponStats.WeaponMesh)
    {
        WeaponMesh->SetStaticMesh(WeaponStats.WeaponMesh);
    }
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();

    // 초기 데이터 기반으로 물리 설정 적용
    InitializeWeaponStats(WeaponStats);

    if (WeaponMesh)
    {
        WeaponMesh->OnComponentHit.AddDynamic(this, &ABaseWeapon::OnWeaponHit);
    }
}

void ABaseWeapon::StartAttack()
{
    LOG_WEAPON(Display, "StartAttack: Mesh Physics Blocking Enabled");

    HitActors.Empty();
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

void ABaseWeapon::ApplyHitStop() // 함수명은 유지하거나 StopOwnerAnimation으로 변경 가능
{
    // 무기 소유자를 BaseCharacter로 캐스팅
    if (ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(GetOwner()))
    {
        StopAttack();

        if (UAnimInstance* AnimInst = OwnerChar->GetMesh()->GetAnimInstance())
        {
            // [수정] 애니메이션 일시 정지가 아닌 현재 재생 중인 몽타주를 즉시 종료
            // 블렌드 아웃 시간(예: 0.1s)을 주면 동작이 좀 더 부드럽게 끊깁니다.
            AnimInst->Montage_Stop(0.3f, OwnerChar->GetCurrentMontage());

            LOG_WEAPON(Display, "Hit Detected: Stopping Attack Montage.");
        }
    }
}

// 상호작용 실행 시 호출
void ABaseWeapon::Interact(ABaseCharacter* Character)
{
    if (Character)
    {
        // 캐릭터의 장착 함수 호출
        Character->EquipWeapon(this);
    }
}

// UI에 표시될 문구
FText ABaseWeapon::GetInteractionPrompt()
{
    return FText::Format(NSLOCTEXT("Interaction", "EquipWeapon", "장착: {0}"), FText::FromString(GetName()));
}

void ABaseWeapon::OnEquipped()
{
    // 장착 시 물리 끄기 및 충돌 설정 변경
    if (WeaponMesh)
    {
        WeaponMesh->SetSimulatePhysics(false);
        WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        bIsEquipped = true;
    }
}

void ABaseWeapon::OnDropped()
{
    // 버릴 때 다시 물리 켜기
    if (WeaponMesh)
    {
        FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
        DetachFromActor(DetachRules);

        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        WeaponMesh->SetSimulatePhysics(true);
        WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        bIsEquipped = false;
    }
}

void ABaseWeapon::InitializeWeaponStats(const FWeaponData& NewStats)
{
    WeaponStats = NewStats;

    if (WeaponMesh && WeaponStats.WeaponMesh)
    {
        // 1. 메시 할당 (WeaponStats에 정의된 스태틱 메시 사용)
        WeaponMesh->SetStaticMesh(WeaponStats.WeaponMesh);

        // 2. 물리 질량 설정 (무게 반영)
        if (WeaponStats.MassKg > 0.0f)  
        {
            WeaponMesh->SetMassOverrideInKg(NAME_None, WeaponStats.MassKg, true);
        }
    }
}

void ABaseWeapon::OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this && OtherActor != GetOwner() && bIsEquipped)
    {
        if (HitActors.Contains(OtherActor)) return;

        // [수정] 기본 데미지(BaseDamage) 기반 데미지 계산
        // 단순히 충격량뿐만 아니라 무기 자체의 공격력을 반영합니다.
        float ImpactForce = NormalImpulse.Size();
        float CalculatedDamage = ImpactForce * DamageCoefficient * 0.01f;

        if(CalculatedDamage < 5.0f)
        {
            UGameplayStatics::ApplyDamage(
                OtherActor,
                CalculatedDamage,
                GetInstigatorController(),
                this,
                UDamageType::StaticClass()
            );
        }

        if (OtherComp && OtherComp->IsSimulatingPhysics())
        {
            LOG_WEAPON(Display, "Add Impulse");

            FVector HitDirection = -Hit.ImpactNormal;
            // 무기 무게가 무거울수록 더 강하게 밀어냄
            OtherComp->AddImpulseAtLocation(HitDirection * ImpactForce * ImpulseCoefficient, Hit.ImpactPoint);
        }

        HitActors.Add(OtherActor);
        ApplyHitStop();
    }
}