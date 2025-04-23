
#include "EditorBillboardRenderPass.h"

#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"
#include "Components/BillboardComponent.h"
#include "Actors/LightActor.h"
#include "Components/Light/LightComponent.h"
#include "LevelEditor/SLevelEditor.h"
#include "Editor/UnrealEd/EditorViewportClient.h"

FEditorBillboardRenderPass::FEditorBillboardRenderPass()
{
    ResourceType = EResourceType::ERT_Editor;
}

void FEditorBillboardRenderPass::PrepareRender()
{
    BillboardComps.Empty();
    for (const auto Component : TObjectRange<UBillboardComponent>())
    {
        if (Component->GetWorld() == GEngine->ActiveWorld && Component->bIsEditorBillboard)
        {

            // override되고 있는 light component에는 billboard가 안뜨도록 수정
            if (ALight* LightActor = Cast<ALight>(Component->GetOwner()))
            {
                if (ULightComponentBase* LightComp = LightActor->GetComponentByClass<ULightComponentBase>())
                {
                    if (LightComp == GEngineLoop.GetLevelEditor()->GetActiveViewportClient()->OverrideLightComponent)
                    {
                        continue;
                    }
                }
            }
            BillboardComps.Add(Component);
        }
    }
}
