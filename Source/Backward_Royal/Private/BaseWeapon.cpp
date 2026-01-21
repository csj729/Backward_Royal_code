#include "BaseWeapon.h"
#include "BaseCharacter.h"
#include "BRGameInstance.h"
#include "Kismet/GameplayStatics.h"

// 로그 매크로
DEFINE_LOG_CATEGORY_STATIC(LogBaseWeapon, Display, All);

#define LOG_WEAPON(Verbosity, Format, ...) \
    UE_LOG(LogBaseWeapon, Verbosity, TEXT("%s - %s"), *FString(__FUNCTION__), *FString::Printf(TEXT(Format), ##__VA_ARGS__))

// static 변수 초기화 (기본값 1.0)

float ABaseWeapon::GlobalDamageMultiplier = 1.0f;
float ABaseWeapon::GlobalImpulseMultiplier = 1.0f;
float ABaseWeapon::GlobalAttackSpeedMultiplier = 1.0f;

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