#include "FLedgeComponentVisualizer.h"
#include "LedgeComponent.h"

void FLedgeComponentVisualizer::DrawVisualization(const UActorComponent* Component,
                                                  const FSceneView* View,
                                                  FPrimitiveDrawInterface* PDI)
{
	auto LedgeComponent = Cast<ULedgeComponent>(Component);
	if (!LedgeComponent)
	{
		return;
	}

	TArray<FLedgeSegment> LedgeSegments;
	LedgeComponent->GetSegments(LedgeSegments);

	const FVector WallHeightOffset = FVector(0., 0., -LedgeComponent->LedgeWallHeight);

	for (const auto& LedgeSegment : LedgeSegments)
	{
		// draw the 'wall' polygon
		TArray<FVector> Verts;
		Verts.Add(LedgeSegment.GetStart());
		Verts.Add(LedgeSegment.GetEnd());
		Verts.Add(LedgeSegment.GetEnd() + WallHeightOffset);
		Verts.Add(LedgeSegment.GetStart() + WallHeightOffset);

		for (int i = 0; i < Verts.Num(); i++)
		{
			PDI->DrawLine(Verts[i], Verts[i + 1], LedgeComponent->LineColor, 0, LedgeComponent->LineThickness);
		}

		// draw the 'floor' polygon (where the character would stand when they climb up)
		Verts.Empty();
		Verts.Add(LedgeSegment.GetStart());
		Verts.Add(LedgeSegment.GetEnd());
		Verts.Add(LedgeSegment.GetEnd() + LedgeSegment.GetOrientation() * LedgeComponent->LedgeDepth * -1.);
		Verts.Add(LedgeSegment.GetStart() + LedgeSegment.GetOrientation() * LedgeComponent->LedgeDepth * -1.);

		for (int i = 0; i < Verts.Num(); i++)
		{
			PDI->DrawLine(Verts[i], Verts[i + 1], LedgeComponent->LineColor, 0, LedgeComponent->LineThickness);
		}
	}
}
