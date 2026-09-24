#include "Misc/AutomationTest.h"
#include "Ballistics.h"
#include "Engine/StaticMeshActor.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrajectoryTests, "UtilityTypes.TrajectoryTests", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

AActor* CreateTestCube(UWorld* World, const FVector& Location)
{
	AStaticMeshActor* Actor = Cast<AStaticMeshActor>(World->SpawnActor(AStaticMeshActor::StaticClass(), &Location));
	Actor->GetStaticMeshComponent()->SetStaticMesh(
		LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	return Actor;
}

bool FTrajectoryTests::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorldWrapper;
	TestWorldWrapper.CreateTestWorld(EWorldType::Type::Game);
	
	FPlanarBallisticTrajectory Trajectory = FPlanarBallisticTrajectory(TestWorldWrapper.GetTestWorld(),
		FVector(100., 100., 100.), FVector(100., 0., 0.), TestWorldWrapper.GetTestWorld()->GetGravityZ());

	this->TestTrue(TEXT("Trajectory passes through the position estimated at a given time"),
	               Trajectory.DoesPassThrough(Trajectory.GetPositionAtTime(1.)));

	Trajectory = FPlanarBallisticTrajectory(TestWorldWrapper.GetTestWorld(),
		FVector(0., 0., 240.), FVector(0., 0.,0.), TestWorldWrapper.GetTestWorld()->GetGravityZ());
	
	this->TestTrue(TEXT("Vertical trajectory should recognize it's vertical"),
		Trajectory.IsVertical());

	this->TestTrue(TEXT("Calibration test"),
		Trajectory.Calibrate(FVector(20., 20., 20.), true));

	this->TestTrue(TEXT("Calibrated trajectory passes through the calibration point"),
		Trajectory.DoesPassThrough(FVector(20., 20., 20.)));

	Trajectory = FPlanarBallisticTrajectory(TestWorldWrapper.GetTestWorld(),
		FVector(100., 100., 100.), FVector(0., 0., 100.), TestWorldWrapper.GetTestWorld()->GetGravityZ());

	this->TestTrue(TEXT("A trajectry can be calibrated to a purely vertical trajectory"),
		Trajectory.Calibrate(FVector(0., 0., 20.), true));

	this->TestTrue(TEXT("A trajectory calibrated to a vertical shot passes through the calibration point"),
		Trajectory.DoesPassThrough(FVector(0., 0., 20.), true));

	this->TestTrue(TEXT("A trajectory can be calibrated to a purely vertical shot downwards"),
		Trajectory.Calibrate(FVector(0., 0., -1000.), true));

	this->TestTrue(TEXT("A trajectory calibrated to a vertical shot below 0. passes through the calibration point"),
		Trajectory.DoesPassThrough(FVector(0., 0., -1000.), true));

	// tests for the algorithm of 2d trajectory discretization
	const FSimpleBallisticTrajectory2D Trajectory2D = FSimpleBallisticTrajectory2D(FVector2D(100., 100.), -9.8);

	constexpr float Tolerance = 0.5;
	const auto Approximation = TrajectoryDiscretization::FPolylineIntervalApproximation::ApproximateTrajectory(
		&Trajectory2D, 0., 100., 100, Tolerance);

	for (int i = 0; i < 100; i++)
	{
		this->TestTrue(TEXT("Approximation is not too far away from the trajectory"),
			FMath::Abs(Approximation.GetError(i * 1.)) <= Tolerance);
	}

	// discretization of the 3d trajectory
	Trajectory = FPlanarBallisticTrajectory(TestWorldWrapper.GetTestWorld(),
		FVector(100., 100., 100.),
		FVector(0., 0., 0.),
		TestWorldWrapper.GetTestWorld()->GetGravityZ());

	const auto DiscretizeStart = Trajectory.GetPositionAtTime(0.5);
	const auto DiscretizeEnd = Trajectory.GetPositionAtTime(1.5);
	
	auto Points = Trajectory.Discretize(DiscretizeStart, DiscretizeEnd, 3, 1.);

	// trajectory should pass through all the discretization points
	for (const auto Pt : Points)
	{
		this->TestTrue(TEXT("Trajectory passes through all the discretization points"),
			Trajectory.DoesPassThrough(Pt, 1e-04)); // TODO why are we losing precision here??
	}

	// we don't check for the equality of intervals - actual number of intervals can be less if tolerance exits earlier
	this->TestTrue(TEXT("Discretized into requested max number of intervals or less"),
		Points.Num() <= 4);

	this->TestTrue(TEXT("First point of discretization must be roughly the discretization interval start"),
		Points[0].Equals(DiscretizeStart));

	this->TestTrue(TEXT("Last point of discretization must be roughly the discretization interval end"),
		Points.Last().Equals(DiscretizeEnd));

	// apex tests
	this->TestTrue(TEXT("A trajectory with positive velocity has an apex"), Trajectory.HasApex());

	FVector Apex;
	Trajectory.GetApex(Apex);
	this->TestTrue(TEXT("The trajectory's highest point is equal to its apex (3d trajectory with positive velocity"),
		Apex.Equals(Trajectory.GetHighestPoint()));

	// calculations related to time required to reach a position
	const FVector PositionToReach = Trajectory.GetPositionAtTime(3.);

	this->TestEqual(TEXT("Basic test for time required to reach"),
		Trajectory.GetPositionAtTime(Trajectory.GetTimeRequiredToReach(PositionToReach)),
		PositionToReach);

	// shape traces
	const FVector LocationWhereToSpawn = FVector(500., 300., -100.);
	auto TestCube = CreateTestCube(TestWorldWrapper.GetTestWorld(), LocationWhereToSpawn);

	Trajectory = FPlanarBallisticTrajectory(TestWorldWrapper.GetTestWorld(),
		FVector(800., 0., 0.),
		FVector::ZeroVector,
		TestWorldWrapper.GetTestWorld()->GetGravityZ());

	this->TestTrue(TEXT("Can be calibrated to hit the test actor"),
		Trajectory.Calibrate(TestCube->GetActorLocation(), false));

	float TimeWhenShouldHitActor = Trajectory.GetTimeRequiredToReach(LocationWhereToSpawn);
	this->TestTrue(TEXT("Time required to reach the test actor is computed"), TimeWhenShouldHitActor > 0.);

	FHitResult HitResult;
	auto HasHit = Trajectory.SweepSingleByChannel(HitResult,
	                                              FVector::ZeroVector,
	                                              Trajectory.GetPositionAtTime(TimeWhenShouldHitActor * 1.5),
	                                              ECollisionChannel::ECC_WorldStatic,
	                                              FCollisionShape::MakeSphere(0.5),
	                                              FQuat::Identity,
	                                              2, 5.);
	
	this->TestTrue(TEXT("The shape trace along the trajectory hits the test actor"), HasHit);
	
	return true;
}