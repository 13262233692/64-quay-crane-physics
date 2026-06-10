#include "QuayCranePhysics.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogQuayCranePhysics);

class FQuayCranePhysicsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogQuayCranePhysics, Log, TEXT("QuayCranePhysics module started"));
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogQuayCranePhysics, Log, TEXT("QuayCranePhysics module shut down"));
    }
};

IMPLEMENT_MODULE(FQuayCranePhysicsModule, QuayCranePhysics)
