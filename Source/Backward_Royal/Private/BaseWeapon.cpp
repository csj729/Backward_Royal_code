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

    bReplicates = true;
    AActor::SetReplicateMovement(true);

    WeaponMesh->SetIsReplicated(true);
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
}

// [핵심] 데이터 테이블에서 정보를 읽어와 적용하는 로직
void ABaseWeapon::LoadWeaponData()
{
    DurabilityReduction = GlobalDurabilityReduction;

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
    LOG_WEAPON(Display, "Weapon Stats Updated -> Name: %s, Mass: %f",
        *WeaponRowName.ToString(), NewStats.MassKg);

    // 화면에 즉시 표시 (디버깅용)
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("Weapon: %s | Mass: %.1f"),
            *WeaponRowName.ToString(), NewStats.MassKg);
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
    if (WeaponMesh)
    {
        // 1. 장착 해제 플래그를 가장 먼저 설정하여 공격 컴포넌트의 접근을 차단
        bIsEquipped = false;

        FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
        DetachFromActor(DetachRules);

        // 2. 물리 및 충돌 강제 초기화
        WeaponMesh->SetSimulatePhysics(true);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

        // 3. 상호작용 채널(Visibility)을 포함한 모든 채널 응답 복구
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeaponMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        WeaponMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        WeaponMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

        // 4. 남아있을지 모르는 델리게이트 정리
        WeaponMesh->OnComponentHit.Clear();

        LOG_WEAPON(Display, "Weapon [%s] dropped: Server-side collision reset complete.", *GetName());
    }
}

// 내구도 감소 로직 구현
void ABaseWeapon::DecreaseDurability(float DamageAmount)
{
    if (CurrentWeaponData.Durability <= 0.0f) return;

    // Version 2: 데미지 비례 (주석)
    // float DurabilityToReduce = DamageAmount * 0.1f;

    CurrentWeaponData.Durability = FMath::Clamp(CurrentWeaponData.Durability - DurabilityReduction, 0.0f, CurrentWeaponData.Durability);

    LOG_WEAPON(Display, "Durability: %.1f / %.1f", CurrentWeaponData.Durability, 100.f);

    // [추가됨] 내구도 0 도달 시 파괴 로직 실행
    if (CurrentWeaponData.Durability <= 0.0f)
    {
        BreakWeapon();
    }
}

void ABaseWeapon::BreakWeapon()
{
    // [2025-11-18] 커스텀 디버그 로그 매크로 사용
    LOG_WEAPON(Warning, "Weapon [%s] has been BROKEN!", *WeaponRowName.ToString());

    // 1. 시각적 처리: 원본 메시를 즉시 숨기고 충돌을 제거
    // Destroy()는 프레임 끝에 수행되므로 시각적으로 즉시 사라지게 해야 겹쳐 보이지 않습니다.
    if (WeaponMesh)
    {
        WeaponMesh->SetVisibility(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetSimulatePhysics(false);
    }

    // 2. 소유자 참조 해제
    ABaseCharacter* OwnerCharacter = Cast<ABaseCharacter>(GetOwner());
    if (OwnerCharacter)
    {
        OwnerCharacter->HandleWeaponBroken();
    }

    //// 3. 장착 해제 및 물리적 분리
    //FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    //DetachFromActor(DetachRules);
    //bIsEquipped = false;

    //// 4. Chaos Destruction 실행
    //if (CurrentWeaponData.FracturedMesh)
    //{
    //    FTransform SpawnTransform = GetActorTransform();
    //    FActorSpawnParameters SpawnParams;
    //    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    //    AGeometryCollectionActor* FracturedActor = GetWorld()->SpawnActor<AGeometryCollectionActor>(
    //        AGeometryCollectionActor::StaticClass(),
    //        SpawnTransform,
    //        SpawnParams
    //    );

    //    if (FracturedActor)
    //    {
    //        UGeometryCollectionComponent* GCComp = FracturedActor->GetGeometryCollectionComponent();
    //        if (GCComp)
    //        {
    //            // 데이터 테이블에서 가져온 파괴 에셋 설정
    //            GCComp->SetRestCollection(CurrentWeaponData.FracturedMesh);

    //            // [수정] 물리 시뮬레이션 활성화 및 히트 이벤트 설정
    //            GCComp->SetSimulatePhysics(true);
    //            GCComp->SetNotifyRigidBodyCollision(true);

    //            // [추가] 조각들이 사방으로 흩어지도록 초기 충격을 가함
    //            // 단순히 스폰만 하면 형태를 유지한 채 떨어질 수 있으므로 임펄스를 추가합니다.
    //            GCComp->AddImpulse(FVector(0.f, 0.f, 50.f)); // 위쪽으로 살짝 튀게 함
    //        }
    //        FracturedActor->SetLifeSpan(10.0f);
    //    }
    //}
    //else
    //{
    //    LOG_WEAPON(Error, "No FracturedMesh defined in DataTable for [%s]!", *WeaponRowName.ToString());
    //    Destroy();
    //}

    // 5. 원본 액터 제거
    Destroy();
}