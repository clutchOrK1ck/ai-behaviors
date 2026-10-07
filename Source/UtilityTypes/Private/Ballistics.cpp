#include "Ballistics.h"

#include "KismetTraceUtils.h"
#include "PhysicsEngine/PhysicsSettings.h"

bool IBallisticTrajectory::GeomSweepSingleIterative(const TArray<FVector>& Points,
                                                    FHitResult& OutHit,
                                                    int& HitSegment,
                                                    const FQuat& Rot,
                                                    const FCollisionShape& CollisionShape,
                                                    ECollisionChannel TraceChannel,
                                                    const FCollisionQueryParams& Params,
                                                    const FCollisionResponseParams& ResponseParams,
                                                    const FCollisionObjectQueryParams& ObjectParams) const
{
	for (int i = 0; i < Points.Num() - 1; i++)
	{
		if (FPhysicsInterface::GeomSweepSingle(GetWorld(),
			CollisionShape,
			Rot,
			OutHit,
			Points[i],
			Points[i+1],
			TraceChannel,
			Params,
			ResponseParams,
			ObjectParams))
		{
			HitSegment = i;
			return true;
		}
	}

	HitSegment = -1;
	return false;
}

bool FVerticalBallisticTrajectory::Calibrate(const double& TargetPosition, bool PreferFastest) 
{
	if (TargetPosition == 0.)
	{
		return false;
	}

	// if the target is below, there's no way we can't hit it
	if (TargetPosition < 0.)
	{
		if (PreferFastest && InitialVelocity > 0. || InitialVelocity < 0. && !PreferFastest)
		{
			InitialVelocity = -InitialVelocity;
		}
		return true;
	}

	// target is above - we should first check if we can reach it with present velocity
	const auto OldInitialVelocity = InitialVelocity;
	InitialVelocity = FMath::Abs(InitialVelocity); // ensure positive velocity for the reach test
		
	if (TargetPosition > GetHighestPoint())
	{
		InitialVelocity = OldInitialVelocity;
		return false;
	}
		
	return true;
}

float FVerticalBallisticTrajectory::GetTimeRequiredToReach(const double& Position) const 
{
	const auto g = FMath::Abs(GravityZ);
		
	const auto TermUnderSqRt = FMath::Square(InitialVelocity) - 2 * g * Position;
	if (TermUnderSqRt < 0.)
	{
		return -1.;
	}

	// for the vertical trajectory, we can have two times - on the way up and on the way down
	const float T1 = (InitialVelocity + FMath::Sqrt(TermUnderSqRt)) / g;
	const float T2 = (InitialVelocity - FMath::Sqrt(TermUnderSqRt)) / g;

	// take the smallest positive time
	const auto PositiveTimes = TArray({T1, T2})
		.FilterByPredicate([](const float Val) { return Val >= 0.; });

	if (PositiveTimes.IsEmpty())
	{
		return -1.;
	}

	return *Algo::MinElement(PositiveTimes);
}

TrajectoryDiscretization::FLinearIntervalApproximation::FLinearIntervalApproximation(const FBallisticTrajectory2D* Trajectory,
			double IntervalStart,
			double IntervalEnd) : Trajectory(Trajectory), IntervalStart(IntervalStart), IntervalEnd(IntervalEnd)
{
	const double y_s = Trajectory->YOfX(IntervalStart),
				 x_s = IntervalStart,
				 y_e = Trajectory->YOfX(IntervalEnd),
				 x_e = IntervalEnd;

	B = (y_s - y_e) / (x_s - x_e);
	A = (y_e * x_s - y_s * x_e) / (x_s - x_e);

	/**
	 * this formula is the 'true' x where the error is largest but it seems from the graph
	 * that for the simple trajectory it's always pretty much in the center of the interval
	 *
	 * XOfLargestError = (Trajectory->GetInitialVelocity().Y / Trajectory->GetInitialVelocity().X - B) * FMath::Square(
		Trajectory->GetInitialVelocity().X) / Trajectory->GetGravityZ();
	 *
	 * so it seems it doesn't make much sense to perform the computations instead of taking the interval center
	 */
			
	// an instance is immutable, so we can cache the values to not recompute every time when requested
	XOfLargestError = IntervalStart + (IntervalEnd - IntervalStart) / 2.;
	LargestError = GetError(XOfLargestError);
}

double TrajectoryDiscretization::FPolylineIntervalApproximation::GetValue(double X) const 
{
	if (X <= Intervals[0].GetIntervalStart())
	{
		return Intervals[0].GetValue(X);
	}

	if (X >= Intervals.Last().GetIntervalEnd())
	{
		return Intervals.Last().GetValue(X);
	}
			
	for (const auto& Interval : Intervals)
	{
		if (X >= Interval.GetIntervalStart() && X <= Interval.GetIntervalEnd())
		{
			return Interval.GetValue(X);
		}
	}

	return -1.;
}

TArray<FVector2D> TrajectoryDiscretization::FPolylineIntervalApproximation::GetPoints() const
{
	TArray<FVector2D> Pts;

	Pts.Add(FVector2D(Intervals[0].GetIntervalStart(), Intervals[0].GetValue(Intervals[0].GetIntervalStart())));

	for (const FLinearIntervalApproximation& Approximation : Intervals)
	{
		Pts.Add(FVector2D(Approximation.GetIntervalEnd(),
						  Approximation.GetValue(Approximation.GetIntervalEnd())));
	}

	return Pts;
}

void TrajectoryDiscretization::FPolylineIntervalApproximation::Subdivide()
{
	FLinearIntervalApproximation* LargestErrorInterval = Algo::MaxElementBy(Intervals, [](const FLinearIntervalApproximation& Interval)
	{
		return Interval.GetLargestApproximationError();
	});

	if (!LargestErrorInterval)
	{
		return;
	}

	const int WorstIntervalIdx = Intervals.Find(*LargestErrorInterval);

	FLinearIntervalApproximation Left, Right;
	LargestErrorInterval->Subdivide(
		LargestErrorInterval->GetXOfLargestApproximationError(),
		Left, Right);

	Intervals[WorstIntervalIdx] = Left;

	if (WorstIntervalIdx == Intervals.Num() - 1)
	{
		Intervals.Add(Right);
	} else
	{
		Intervals.Insert(Right, WorstIntervalIdx + 1);
	}
}

TrajectoryDiscretization::FPolylineIntervalApproximation TrajectoryDiscretization::FPolylineIntervalApproximation::ApproximateTrajectory(const FBallisticTrajectory2D* Trajectory,
															double IntervalStart,
															double IntervalEnd,
															int MaxIntervals,
															float Tolerance)
{
	FPolylineIntervalApproximation Approximation(Trajectory, IntervalStart, IntervalEnd);
	while (Approximation.GetLargestApproximationError() > Tolerance && Approximation.GetNumIntervals() < MaxIntervals)
	{
		Approximation.Subdivide();
	}
	return Approximation;
}

bool FSimpleBallisticTrajectory2D::DoesPassThrough(const FVector2D& Position, double Tolerance) const 
{
	// an immediate simple test is whether x is reachable with the initial x-velocity
	if (Position.X < 0.)
	{
		return false;
	}
		
	// handle vertical trajectory case
	if (IsVertical())
	{
		return FMath::IsNearlyZero(Position.X) &&
			FVerticalBallisticTrajectory(InitialVelocity.Y, GravityZ).DoesPassThrough(Position.Y, Tolerance);
	}
		
	return FMath::IsNearlyEqual(Position.Y, YOfX(Position.X), Tolerance);
}

float FSimpleBallisticTrajectory2D::GetTimeRequiredToReach(const FVector2D& Position) const 
{
	if (!DoesPassThrough(Position))
	{
		return -1.;
	}

	// handle vertical trajectory case
	if (IsVertical())
	{
		return FVerticalBallisticTrajectory(InitialVelocity.Y, GravityZ).GetTimeRequiredToReach(Position.Y);
	}
		
	// without the drag, we can calculate the time-to-reach from x-coords only
	return Position.X / InitialVelocity.X;
}

bool FSimpleBallisticTrajectory2D::Calibrate(const FVector2D& TargetPosition, bool PreferFastest) 
{
	if (TargetPosition.IsZero() || TargetPosition.X < 0.)
	{
		return false;
	}

	const auto v_0 = GetInitialVelocity().Length();

	// purely vertical shot - see if we can reach that with a 1D vertical ballistic trajectory
	if (TargetPosition.X == 0.)
	{
		if (auto VerticalTrajectory = FVerticalBallisticTrajectory(v_0, GravityZ); VerticalTrajectory.Calibrate(TargetPosition.Y, PreferFastest))
		{
			InitialVelocity = FVector2D(0., VerticalTrajectory.GetInitialVelocity());
			return true;
		}

		return false;
	}
		
	const auto g = FMath::Abs(GetGravityZ());
	const auto y = TargetPosition.Y;
	const auto x = TargetPosition.X;

	const auto TermUnderSqRoot = FMath::Pow(v_0, 4) - g * (g * FMath::Pow(x, 2) + 2 * FMath::Pow(v_0, 2) * y);

	if (TermUnderSqRoot < 0.)
	{
		return false;
	}

	if (TermUnderSqRoot == 0.)
	{
		const auto LaunchAngle = FMath::Atan(FMath::Pow(v_0, 2) / (g * x));

		InitialVelocity = FVector2D(
			v_0 * FMath::Cos(LaunchAngle),
			v_0 * FMath::Sin(LaunchAngle));
			
		return true;
	}

	const auto LaunchAngleHighArc = FMath::Atan((FMath::Pow(v_0, 2) + FMath::Sqrt(TermUnderSqRoot)) / (g * x));
	const auto LaunchAngleLowArc = FMath::Atan((FMath::Pow(v_0, 2) - FMath::Sqrt(TermUnderSqRoot)) / (g * x));

	if (PreferFastest)
	{
		// low arc is always faster
		*this = FSimpleBallisticTrajectory2D(
			FVector2D(
				v_0 * FMath::Cos(LaunchAngleLowArc),
				v_0 * FMath::Sin(LaunchAngleLowArc)), GravityZ);
	} else
	{
		*this = FSimpleBallisticTrajectory2D(
			FVector2D(
				v_0 * FMath::Cos(LaunchAngleHighArc),
				v_0 * FMath::Sin(LaunchAngleHighArc)), GravityZ);
	}

	return true;
}

TArray<FVector2D> FSimpleBallisticTrajectory2D::Discretize(const FVector2D& Start,
		const FVector2D& End,
		int MaxNumberOfIntervals,
		float Tolerance) const 
{
	if (IsVertical())
	{
		// we could delegate to 1d but why complicate
		// we also assume user supplied valid start and end lying both on the trajectory
		return TArray(
			{
				FVector2D(0., Start.Y),
				FVector2D(0., End.Y)
			});
	}
	else
	{
		return TrajectoryDiscretization::FPolylineIntervalApproximation::ApproximateTrajectory(
			this, Start.X, End.X, MaxNumberOfIntervals, Tolerance).GetPoints();
	}
}

FPlanarBallisticTrajectory::FPlanarBallisticTrajectory(
		UWorld* World,
		const FVector& InitialVelocity,
		const FVector& Origin,
		const float GravityZ
		) :
		InitialVelocity(InitialVelocity), Origin(Origin), GravityZ(GravityZ), World(World)
{
	checkf(World != nullptr, TEXT("Expected a valid world for ballistic trajectory"));
	checkf(GravityZ < 0., TEXT("Expected negative number for gravity"));
	checkf(!InitialVelocity.IsZero(), TEXT("Expected a non-zero velocity for the trajectory"));
		
	const auto VelocityZ = InitialVelocity.Z;
	const auto VelocityXY = FVector::VectorPlaneProject(InitialVelocity, FVector::UpVector).Length();
		
	// an actual implementation depends on drag properties and such, for the moment we're using the basic implementation
	Helper2DImpl = MakeShareable<FSimpleBallisticTrajectory2D>(new FSimpleBallisticTrajectory2D(FVector2D(VelocityXY, VelocityZ), GravityZ));
}

TOptional<FPlanarBallisticTrajectory> FPlanarBallisticTrajectory::CreateChecked(UWorld* World, const FVector& Origin, const FVector& InitialVelocity, const float GravityZ)
{
	if (!World || InitialVelocity.IsZero() || GravityZ >= 0.)
	{
		return {};
	}

	return FPlanarBallisticTrajectory(World, InitialVelocity, Origin, GravityZ);
}

TOptional<FPlanarBallisticTrajectory> FPlanarBallisticTrajectory::CreateChecked(const UObject* WorldContextObject, const FVector& Origin, const FVector& InitialVelocity)
{
	if (!WorldContextObject || !WorldContextObject->GetWorld()) {
		return {};
	}
	return CreateChecked(WorldContextObject->GetWorld(), Origin, InitialVelocity, WorldContextObject->GetWorld()->GetGravityZ());
}

float FPlanarBallisticTrajectory::GetTimeRequiredToReach(const FVector& Position) const 
{
	if (!DoesPassThrough(Position))
	{
		return -1.;
	}
	return Helper2DImpl->GetTimeRequiredToReach(WorldPosToTrajectory2DLocalPos(Position));
}

bool FPlanarBallisticTrajectory::DoesPassThrough(const FVector& Position, double Tolerance) const 
{
	if (Position.Equals(Origin))
	{
		return true;
	}
		
	// if shot direction does not point at position's XY, we obviously don't hit it
	if (!GetShotDirectionXY().Equals((Position - Origin).GetSafeNormal2D()))
	{
		return false;
	}
	return Helper2DImpl->DoesPassThrough(WorldPosToTrajectory2DLocalPos(Position), Tolerance);
}

bool FPlanarBallisticTrajectory::Calibrate(const FVector& TargetPosition, bool PreferFastest) 
{
	if ((TargetPosition - Origin).IsZero())
	{
		return false;
	}
		
	// if we ever hope to hit the target, we should direct the shot towards it (unless the shot is vertical which is handled by traj2d)
	const auto TargetDirection = (TargetPosition - Origin).GetSafeNormal2D();
	const auto TargetDistanceXY = FVector::VectorPlaneProject(TargetPosition - Origin, FVector::UpVector);

	// check if this would be reachable in the 2d trajectory
	const auto TargetPositionIn2DTrajectorySpace = FVector2D(
		TargetDistanceXY.Length(),
		(TargetPosition - Origin).Z
	);

	if (Helper2DImpl->Calibrate(TargetPositionIn2DTrajectorySpace, PreferFastest))
	{
		InitialVelocity = TargetDirection * Helper2DImpl->GetInitialVelocity().X + FVector::UpVector * Helper2DImpl->GetInitialVelocity().Y;
		return true;
	}

	return false;
}

TArray<FVector> FPlanarBallisticTrajectory::Discretize(const FVector& Start,
		const FVector& End,
		int MaxNumberOfIntervals,
		float Tolerance) const 
{
	TArray<FVector> Pts;

	FVector2D Start2D = WorldPosToTrajectory2DLocalPos(Start);
	FVector2D End2D = WorldPosToTrajectory2DLocalPos(End);
		
	Algo::Transform(Helper2DImpl->Discretize(Start2D, End2D, MaxNumberOfIntervals, Tolerance), Pts, [this](const FVector2D& Pos2D)
	{
		return this->Trajectory2DLocalPosToWorldPos(Pos2D);
	});

	return Pts;
}

FBallisticTrajectory UBallisticTrajectoryLibrary::FindPassThroughTrajectory(bool& Success, const UObject* WorldContextObject,
	const FVector& Origin,
	const float Velocity, const FVector& PassThroughLocation, const bool PreferShortArc)
{
	// TODO different implementations? but for the moment just the planar trajectory
	auto Trajectory = FPlanarBallisticTrajectory::CreateChecked(WorldContextObject, Origin, FVector{Velocity, 0., 0.});
	if (!Trajectory || !Trajectory->Calibrate(PassThroughLocation, PreferShortArc))
	{
		Success = false;
		return FBallisticTrajectory();
	}

	Success = true;
	return FBallisticTrajectory{Trajectory->GetInitialVelocity(), Trajectory->GetOrigin(), Trajectory->GetGravityZ()};
}

void DrawDebugSphereTraceSingleOnTrajectory(UWorld* World,
                                            const TArray<FVector>& Points,
                                            float Radius,
                                            EDrawDebugTrace::Type DrawDebugType,
                                            bool bHit,
                                            const FHitResult& OutHit,
                                            int HitSegment,
                                            FLinearColor TraceColor,
                                            FLinearColor TraceHitColor,
                                            float DrawTime)
{
	if (DrawDebugType != EDrawDebugTrace::None && Points.Num() >= 2)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = DrawDebugType == EDrawDebugTrace::ForDuration ? DrawTime : 0.f;
		
		if (!bHit)
		{
			// green spheres along the trajectory
			for (int i = 1; i < Points.Num(); i++)
			{
				DrawDebugSweptSphere(World, Points[i-1], Points[i], Radius, TraceColor.ToFColor(true), bPersistent, LifeTime);
			}
		} else
		{
			// green spheres up to hit, red afterwards + the point of the impact
			for (int i = 0; i < Points.Num() - 1; i++)
			{
				if (i < HitSegment) // green
				{
					DrawDebugSweptSphere(World, Points[i], Points[i+1], Radius, TraceColor.ToFColor(true), bPersistent, LifeTime);
				} else if (i == HitSegment)
				{
					// green up to hit point, red after
					DrawDebugSweptSphere(World, Points[i], OutHit.Location, Radius, TraceColor.ToFColor(true), bPersistent, LifeTime);
					DrawDebugSweptSphere(World, OutHit.Location, Points[i+1], Radius, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
					DrawDebugPoint(World, OutHit.ImpactPoint, 16.f, TraceColor.ToFColor(true), bPersistent, LifeTime);
				} else
				{
					DrawDebugSweptSphere(World, Points[i], Points[i+1], Radius, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
				}
			}
		}
	}
}

// there's a similar function in the Engine module but it is not exported by the module
FCollisionQueryParams ConfigureCollisionParams(FName TraceTag, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, bool bIgnoreSelf, const UObject* WorldContextObject)
{
	// TODO what's the deal with the stat id here?
	FCollisionQueryParams Params(TraceTag, SCENE_QUERY_STAT_ONLY(TrajectoryTraces), bTraceComplex);

	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		const AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			const UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	return Params;
}

bool UBallisticTrajectoryLibrary::SphereTraceSingleOnTrajectory(const UObject* WorldContextObject,
                                                                const FBallisticTrajectory& Trajectory,
                                                                const float EndTime,
                                                                const float Tolerance,
                                                                const int MaxLinearizeIntervals,
                                                                float Radius,
                                                                ETraceTypeQuery TraceChannel,
                                                                bool bTraceComplex,
                                                                const TArray<AActor*>& ActorsToIgnore,
                                                                EDrawDebugTrace::Type DrawDebugType,
                                                                FHitResult& OutHit,
                                                                bool bIgnoreSelf,
                                                                FLinearColor TraceColor,
                                                                FLinearColor TraceHitColor,
                                                                float DrawTime)
{
	// TODO other trajectories but for the moment being we only have the planar ballistic
	auto Traj = FPlanarBallisticTrajectory::CreateChecked(WorldContextObject, Trajectory.Origin,
	                                                      Trajectory.InitialVelocity);
	if (!Traj)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid input trajectory for sphere trace on trajectory"));
		return false;
	}

	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	static const FName SphereTraceOnTrajectorySingleName(TEXT("SphereTraceSingleOnTrajectory"));
	FCollisionQueryParams Params = ConfigureCollisionParams(SphereTraceOnTrajectorySingleName, bTraceComplex, ActorsToIgnore, bIgnoreSelf, WorldContextObject);

	TArray<FVector> DiscretizationPoints;
	int HitSegment;
	
	const bool bHit = Traj->SweepSingleByChannel(OutHit,
	                                             DiscretizationPoints,
	                                             HitSegment,
	                                             Traj->GetPositionAtTime(0.f),
	                                             Traj->GetPositionAtTime(EndTime),
	                                             CollisionChannel,
	                                             FCollisionShape::MakeSphere(Radius),
	                                             FQuat::Identity,
	                                             MaxLinearizeIntervals,
	                                             Tolerance,
	                                             Params);

#if ENABLE_DRAW_DEBUG
	DrawDebugSphereTraceSingleOnTrajectory(Traj->GetWorld(), DiscretizationPoints, Radius, DrawDebugType, bHit, OutHit, HitSegment, TraceColor, TraceHitColor, DrawTime);
#endif
	
	return bHit;
}


