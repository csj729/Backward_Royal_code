#include "BaseWeapon.h"
#include "BaseCharacter.h"
#include "BRGameInstance.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"

// 로그 매크로
DEFINE_LOG_CATEGORY_STATIC(LogBaseWeapon, Display, All);

#define LOG_WEAPON(Verbosity, Format, ...) \
    UE_LOG(LogBaseWeapon, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(TEXT(Format), ##__VA_ARGS__))

// static 변수 초기화 (기본값 1.0)
float ABaseWeapon::GlobalDamageMultiplier = 1.0f;
float ABaseWeapon::GlobalImpulseMultiplier = 1.0f;
float ABaseWeapon::GlobalAttackSpeedMultiplier = 1.0f;
float ABaseWeapon::GlobalDurabilityReduction = 10.0f;

ABaseWeapon::ABaseWeapon()
{
    PrimaryActorTick.bCanEverTick = false;
    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
    RootComponent = WeaponMesh;

    // 기본 충돌 및 물리 설정
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

    bReplicates = true;
    AActor::SetReplicateMovement(true);

    WeaponMesh->SetIsReplicated(true);
}

void ABaseWeapon::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    LoadWeaponData();
}

void ABaseWeapon::BeginPlay()
{
    Super::BeginPlay();
    LoadWeaponData();
}

void ABaseWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (WeaponMesh)
    {
        WeaponMesh->SetSimulatePhysics(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    }
}

void ABaseWeapon::LoadWeaponData()
{
    DurabilityReduction = GlobalDurabilityReduction;

    UBRGameInstance* GI = Cast<UBRGameInstance>(GetGameInstance());
    if (!GI) return;

    UDataTable* WeaponTable = nullptr;
    FString TableKey = TEXT("WeaponData");

    if (GI->ConfigDataMap.Contains(TableKey))
    {
        WeaponTable = GI->ConfigDataMap[TableKey];
    }

    if (!WeaponTable)
    {
        LOG_WEAPON(Warning, "WeaponDataTable Not Found in GameInstance with Key: %s", *TableKey);
        return;
    }

    if (WeaponRowName.IsNone()) return;

    static const FString ContextString(TEXT("Weapon Data Context"));
    FWeaponData* FoundData = WeaponTable->FindRow<FWeaponData>(WeaponRowName, ContextString);

    if (FoundData)
    {
        CurrentWeaponData = *FoundData;
        InitializeWeaponStats(CurrentWeaponData);
        LOG_WEAPON(Display, "Loaded Weapon Data for [%s] from GameInstance", *WeaponRowName.ToString());
    }
    else
    {
        LOG_WEAPON(Warning, "Failed to find Row [%s] in the GameInstance's Weapon Table", *WeaponRowName.ToString());
    }
}

void ABaseWeapon::InitializeWeaponStats(const FWeaponData& NewStats)
{
    LOG_WEAPON(Display, "Weapon Stats Updated -> Name: %s, Mass: %f", *WeaponRowName.ToString(), NewStats.MassKg);

    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Weapon: %s | Mass: %.1f"), *WeaponRowName.ToString(), NewStats.MassKg);
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, DebugMsg);
    }

    if (WeaponMesh && NewStats.WeaponMesh)
    {
        WeaponMesh->SetStaticMesh(NewStats.WeaponMesh);
        if (NewStats.MassKg > 0.0f)
        {
            WeaponMesh->SetMassOverrideInKg(NAME_None, NewStats.MassKg, true);
        }
    }
}

void ABaseWeapon::Interact(ABaseCharacter* Character)
{
    if (Character)
    {
        Character->EquipWeapon(this);
    }
}

FText ABaseWeapon::GetInteractionPrompt()
{
    return FText::Format(NSLOCTEXT("Interaction", "EquipWeapon", "장착: {0}"), FText::FromString(GetName()));
}

void ABaseWeapon::OnEquipped()
{
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
    if (WeaponMesh)
    {
        bIsEquipped = false;

        FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
        DetachFromActor(DetachRules);

        WeaponMesh->SetSimulatePhysics(true);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeaponMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        WeaponMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        WeaponMesh->OnComponentHit.Clear();

        LOG_WEAPON(Display, "Weapon [%s] dropped: Server-side collision reset complete.", *GetName());
    }
}

void ABaseWeapon::DecreaseDurability(float DamageAmount)
{
    if (CurrentWeaponData.Durability <= 0.0f) return;

    CurrentWeaponData.Durability = FMath::Clamp(CurrentWeaponData.Durability - DurabilityReduction, 0.0f, CurrentWeaponData.Durability);
    LOG_WEAPON(Display, "Durability: %.1f / %.1f", CurrentWeaponData.Durability, 100.f);

    if (CurrentWeaponData.Durability <= 0.0f)
    {
        BreakWeapon();
    }
}

void ABaseWeapon::BreakWeapon()
{
    LOG_WEAPON(Warning, "Weapon [%s] has been BROKEN!", *WeaponRowName.ToString());

    // 1. 시각적 처리 숨기기
    if (WeaponMesh)
    {
        WeaponMesh->SetVisibility(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetSimulatePhysics(false);
    }

    // 2. 소유자 해제
    ABaseCharacter* OwnerCharacter = Cast<ABaseCharacter>(GetOwner());
    if (OwnerCharacter)
    {
        OwnerCharacter->HandleWeaponBroken();
    }

    // 3. Transform 저장 및 장착 해제
    FTransform SpawnTransform = GetActorTransform();
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    DetachFromActor(DetachRules);
    bIsEquipped = false;

    // 4. [수정됨] 권한이 있는 서버에서만 멀티캐스트를 호출하여 클라이언트들에게 파편 스폰 명령을 내림
    if (HasAuthority())
    {
        // 서버의 CurrentWeaponData.FracturedMesh를 직접 인자로 넘겨 클라이언트 Null 참조를 방지
        Multicast_BreakWeaponVisual(SpawnTransform, CurrentWeaponData.FracturedMesh);

        // 멀티캐스트 RPC가 클라이언트들에게 도달할 시간을 주기 위해 지연 시간 연장 (0.2 -> 0.5)
        SetLifeSpan(0.5f);
    }
}

// [수정됨] 매개변수로 서버에서 넘겨준 InFracturedMesh를 사용하도록 변경
void ABaseWeapon::Multicast_BreakWeaponVisual_Implementation(const FTransform& SpawnTransform, UGeometryCollection* InFracturedMesh)
{
    LOG_WEAPON(Display, "Multicast_BreakWeaponVisual Called. Mesh Valid: %s", InFracturedMesh ? TEXT("True") : TEXT("False"));

    if (InFracturedMesh)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AGeometryCollectionActor* FracturedActor = GetWorld()->SpawnActorDeferred<AGeometryCollectionActor>(
            AGeometryCollectionActor::StaticClass(),
            SpawnTransform
        );

        if (FracturedActor)
        {
            // 각 클라이언트에서 로컬로 연산할 것이므로 서버의 파편 물리 상태를 리플리케이트 받지 않음 (네트워크 부하 극저하)
            FracturedActor->SetReplicates(false);

            UGeometryCollectionComponent* GCComp = FracturedActor->GetGeometryCollectionComponent();
            if (GCComp)
            {
                // 클라이언트 환경에서도 서버가 넘겨준 포인터를 통해 정상적으로 설정됨
                GCComp->SetRestCollection(InFracturedMesh);

                // 1. 파괴가 100% 보장되는 기본 물리 프로파일 사용
                GCComp->SetCollisionProfileName(TEXT("PhysicsActor"));

                // 2. 그 위에 덮어쓰기: 파편이 플레이어의 길을 막거나 튕겨내지 않도록 무시
                GCComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
                GCComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

                GCComp->SetSimulatePhysics(true);
                GCComp->SetNotifyRigidBodyCollision(true);
            }

            // FinishSpawningActor를 호출하여 액터 초기화 완료
            UGameplayStatics::FinishSpawningActor(FracturedActor, SpawnTransform);

            if (GCComp)
            {
                // 스폰이 완료된 이후에 물리 상태를 안전하게 재생성
                GCComp->RecreatePhysicsState();

                // 타겟 액터에만 점 데미지를 주어 제자리에서 즉시 분리
                FHitResult HitInfo;
                HitInfo.ImpactPoint = SpawnTransform.GetLocation();

                UGameplayStatics::ApplyPointDamage(
                    FracturedActor,
                    1000000.f,
                    FVector::DownVector,
                    HitInfo,
                    nullptr,
                    this,
                    nullptr
                );

                // 아주 살짝 흩어지도록 임펄스 추가
                FVector CrumbleImpulse = FMath::VRand() * 10.0f;
                GCComp->AddImpulse(CrumbleImpulse, NAME_None, true);
            }
            FracturedActor->SetLifeSpan(10.0f);
        }
    }
    else
    {
        LOG_WEAPON(Warning, "InFracturedMesh is NULL! Multicast failed to spawn GC on this client.");
    }
}