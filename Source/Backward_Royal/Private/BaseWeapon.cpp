#include "BaseWeapon.h"
#include "BaseCharacter.h"
#include "BRGameInstance.h"
#include "Kismet/GameplayStatics.h"

// 로그 매크로
DEFINE_LOG_CATEGORY_STATIC(LogBaseWeapon, Display, All);

#define LOG_WEAPON(Verbosity, Format, ...) \
    UE_LOG(LogBaseWeapon, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(TEXT(Format), ##__VA_ARGS__))

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    // 기본 충돌 및 물리 설정 (기존과 동일)
    WeaponMesh->SetNotifyRigidBodyCollision(true);
    WeaponMesh->SetCollisionProfileName(TEXT("Custom"));
    WeaponMesh->SetSimulatePhysics(true);
    WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    WeaponMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECR_Block);
    WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    bIsEquipped = false;
    GripSocketName = TEXT("Grip");
    DamageCoefficient = 1.0f;
    ImpulseCoefficient = 1.0f;
    AttackSpeedCoefficient = 1.0f;
}

// 에디터에서 수치(RowName 등) 변경 시 즉시 반영
void ABaseWeapon::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    LoadWeaponData();
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();

    // 게임 시작 시 최신 데이터 로드 및 적용
    LoadWeaponData();

    if (WeaponMesh)
    {
        WeaponMesh->OnComponentHit.AddDynamic(this, &ABaseWeapon::OnWeaponHit);
    }
}

// [핵심] 데이터 테이블에서 정보를 읽어와 적용하는 로직
void ABaseWeapon::LoadWeaponData()
{
    // 1. 테이블과 행 이름이 유효한지 확인
    if (MyDataTable && !WeaponRowName.IsNone())
    {
        static const FString ContextString(TEXT("Weapon Data Context"));

        // 2. 테이블에서 직접 행을 찾음. 
        // 이 시점에 MyDataTable은 이미 GameInstance에 의해 JSON 수치로 업데이트된 상태입니다.
        FWeaponData* FoundData = MyDataTable->FindRow<FWeaponData>(WeaponRowName, ContextString);

        if (FoundData)
        {
            CurrentWeaponData = *FoundData;

            // 3. 실제 컴포넌트(메시, 질량 등)에 수치 적용
            InitializeWeaponStats(CurrentWeaponData);

            LOG_WEAPON(Display, "Successfully applied JSON-Balanced data for Row: %s", *WeaponRowName.ToString());
        }
        else
        {
            LOG_WEAPON(Warning, "Failed to find Row [%s] in DataTable [%s]",
                *WeaponRowName.ToString(), *MyDataTable->GetName());
        }
    }
}

void ABaseWeapon::InitializeWeaponStats(const FWeaponData& NewStats)
{
    LOG_WEAPON(Display, "Weapon Stats Updated -> Name: %s, Damage: %f, Mass: %f",
        *WeaponRowName.ToString(), NewStats.BaseDamage, NewStats.MassKg);

    // 화면에 즉시 표시 (디버깅용)
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Weapon: %s | Damage: %.1f | Mass: %.1f"),
            *WeaponRowName.ToString(), NewStats.BaseDamage, NewStats.MassKg);
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, DebugMsg);
    }

    // NewStats는 이제 데이터 테이블에서 가져온 행 데이터임
    if (WeaponMesh && NewStats.WeaponMesh)
    {
        // 1. 메시 할당
        WeaponMesh->SetStaticMesh(NewStats.WeaponMesh);

        // 2. 물리 질량 설정 (데이터 테이블의 MassKg 반영)
        if (NewStats.MassKg > 0.0f)
        {
            WeaponMesh->SetMassOverrideInKg(NAME_None, NewStats.MassKg, true);
        }
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

void ABaseWeapon::OnWeaponHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    if (OtherActor && OtherActor != this && OtherActor != GetOwner() && bIsEquipped)
    {
        if (HitActors.Contains(OtherActor)) return;

        // 데이터 테이블에서 가져온 BaseDamage를 기반으로 계산
        float ImpactForce = NormalImpulse.Size();
        // 실제로는 NewStats.BaseDamage를 사용하는 것이 더 정확함 (CurrentWeaponData.BaseDamage)
        float CalculatedDamage = ImpactForce * DamageCoefficient * 0.01f;

        UGameplayStatics::ApplyDamage(
            OtherActor,
            CalculatedDamage,
            GetInstigatorController(),
            this,
            UDamageType::StaticClass()
        );

        if (OtherComp && OtherComp->IsSimulatingPhysics())
        {
            FVector HitDirection = -Hit.ImpactNormal;
            // ImpulseCoefficient와 충격량 결합
            OtherComp->AddImpulseAtLocation(HitDirection * ImpactForce * ImpulseCoefficient, Hit.ImpactPoint);
        }

        HitActors.Add(OtherActor);
        ApplyHitStop();
    }
}