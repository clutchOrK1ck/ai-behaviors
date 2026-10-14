#pragma once

#include "ComponentVisualizer.h"

/**
 * visualizes the ledge in the viewport
 */
class FLedgeComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;
};
