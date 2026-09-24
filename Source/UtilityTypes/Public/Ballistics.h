#pragma once

#include "CoreMinimal.h"
#include "Algo/MaxElement.h"

template<typename T>
class TBallisticTrajectory
{
public:
	virtual T GetInitialVelocity() const = 0;
	virtual T GetOrigin() const = 0;
	virtual T GetPositionAtTime(float Time) const = 0;
	virtual T GetVelocityAtTime(float Time) const = 0;
	virtual T GetAccelerationAtTime(float Time) const = 0;

	/**
	 * launch angle in radians
	 */
	virtual float GetLaunchAngle() const = 0;

	/**
	 * @return a point where vertical velocity == 0.
	 */
	bool GetApex(T& OutValue) const
	{
		if (const float TimeAtApex = GetTimeAtApex(); TimeAtApex < 0.)
		{
			return false;
		}
		
		OutValue = GetPositionAtTime(GetTimeAtApex());
		return true;
	}

	T GetHighestPoint() const
	{
		if (IsPointingDown())
		{
			return GetOrigin();
		}
		
		T Apex;
		GetApex(Apex);
		return Apex;
	}

	/**
	 * whether this trajectory has an apex, i.e. a point where vertical velocity = 0. (projectile stops for an instant)
	 */
	bool HasApex() const
	{
		return GetTimeAtApex() >= 0.;
	}
	
	virtual bool DoesPassThrough(const T& Position, double Tolerance = 1e-08) const = 0;
	virtual bool IsVertical() const = 0; // whether this trajectory has only Z-velocity while other components are zero

	/**
	 * gets the time required to reach the position. if position is not reachable at all, returns a negative value
	 * @param Position
	 * @return 
	 */
	virtual float GetTimeRequiredToReach(const T& Position) const = 0;
	virtual float GetTimeAtApex() const = 0;
	virtual float GetGravityZ() const = 0;

	/**
	 * calibrate the trajectory so that is passes through the target position
	 * @param TargetPosition position to 'hit'
	 * @param PreferFastest prefer short arc over the long arc
	 * @return true if calibration was successful and false if the trajectory cannot pass through the desired point with the given initial velocity
	 */
	virtual bool Calibrate(const T& TargetPosition, bool PreferFastest) = 0;

	/**
	 * if this trajectory has a positive launch angle (points up)
	 */
	bool IsPointingUp() const
	{
		return GetLaunchAngle() > 0.;
	}

	/**
	 * whether this trajectory has a negative launch angle (points down)
	 */
	bool IsPointingDown() const
	{
		return GetLaunchAngle() < 0.;
	}

	/**
	 * discretizes a piece of the trajectory defined by Start and End which must belong to the trajectory
	 * 
	 * @param Start
	 * @param End 
	 * @param Tolerance allowed distance between the linear approximation and the actual trajectory, in cm
	 * @param MaxNumberOfIntervals maximum number of intervals for discretization. The actual number can be less depending on tolerance
	 * @return 
	 */
	virtual TArray<T> Discretize(const T& Start, const T& End, int MaxNumberOfIntervals, float Tolerance = 1.) const = 0;
	
	virtual ~TBallisticTrajectory() {}
};

class IBallisticTrajectory : public TBallisticTrajectory<FVector>
{

	bool GeomSweepSingleIterative(const TArray<FVector>& Points,
		FHitResult& OutHit,
		const FQuat& Rot,
		const FCollisionShape& CollisionShape,
		ECollisionChannel TraceChannel,
		const FCollisionQueryParams& Params,
		const FCollisionResponseParams& ResponseParams,
		const FCollisionObjectQueryParams& ObjectParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const
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
				return true;
			}
		}

		return false;
	}
	
public:
	// get the world this trajectory exists in
	virtual UWorld* GetWorld() const = 0;
	
	/**
	 * sweep a shape along the trajectory
	 *
	 * this will actually perform multiple linear sweeps, constrained by MaxIterations
	 * @param OutHit			the first blocking hit
	 * @param Start				point on the trajectory where the sweep starts
	 * @param End				point on the trajectory where the sweep ends
	 * @param TraceChannel
	 * @param CollisionShape 
	 * @param Params 
	 * @param ResponseParams
	 * @param MaxIterations		maximum for the number of performed linear sweeps, for performance considerations
	 * @param Tolerance			how much linear approximations are allowed to deviate from the trajectory
	 * @return 
	 */
	bool SweepSingleByChannel(FHitResult& OutHit,
	                          const FVector& Start,
	                          const FVector& End,
	                          ECollisionChannel TraceChannel,
	                          const FCollisionShape& CollisionShape,
	                          const FQuat& Rot,
	                          const int MaxIterations,
							  const float Tolerance,
	                          const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
	                          const FCollisionResponseParams& ResponseParams = FCollisionResponseParams::DefaultResponseParam) const
	{
		return GeomSweepSingleIterative(Discretize(Start, End, MaxIterations, Tolerance), OutHit, Rot, CollisionShape, TraceChannel, Params, ResponseParams);
	}
};

/**
 * a basic vertical trajectory with origin @ 0
 */
class FVerticalBallisticTrajectory : public TBallisticTrajectory<double>
{
	double InitialVelocity;
	const float GravityZ;

public:
	FVerticalBallisticTrajectory(const double InitialVelocity, const float GravityZ) : InitialVelocity(InitialVelocity),
		GravityZ(GravityZ)
	{
		checkf(GravityZ < 0., TEXT("Expected a negative number for gravity"));
	}

	virtual bool IsVertical() const override
	{
		return true;
	}

	virtual double GetInitialVelocity() const override
	{
		return InitialVelocity;
	}

	virtual float GetGravityZ() const override
	{
		return GravityZ;
	}

	virtual double GetOrigin() const override
	{
		return 0.f;
	}
	
	virtual double GetAccelerationAtTime(float Time) const override
	{
		return GetGravityZ();
	}

	virtual double GetVelocityAtTime(float Time) const override
	{
		return InitialVelocity + GetGravityZ() * Time;
	}

	virtual double GetPositionAtTime(float Time) const override
	{
		return InitialVelocity * Time + FMath::Square(Time) / 2. * GetGravityZ();
	}

	virtual bool DoesPassThrough(const double& Position, double Tolerance = 1e-08) const override
	{
		return Position <= GetHighestPoint();
	}

	virtual bool Calibrate(const double& TargetPosition, bool PreferFastest) override
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

	/**
	 * returns the shortest time required to reach a position. if the position is not reachable with given velocity, returns a negative value
	 * @param Position 
	 * @return 
	 */
	virtual float GetTimeRequiredToReach(const double& Position) const override
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
	
	virtual float GetTimeAtApex() const override
	{
		return InitialVelocity / FMath::Abs(GravityZ);
	}

	virtual float GetLaunchAngle() const override
	{
		// we consider a shot up to be 90deg and a shot down negative 90deg
		return InitialVelocity > 0. ? PI / 2. : - PI / 2.;
	}

	/**
	 * in case of a vertical trajectory, start and end define an interval on the y-axis
	 */
	virtual TArray<double> Discretize(const double& Start,
	                                  const double& End,
	                                  int MaxNumberOfIntervals,
	                                  float Tolerance) const override
	{
		// since there's no non-linearity, we return start and end of the interval itself
		return TArray({Start, End});
	}
};

class FBallisticTrajectory2D : public TBallisticTrajectory<FVector2D>
{
public:

	/**
	 * this trajectory's y (vertical position) expressed as a function of x (horizontal position)
	 *
	 * be aware that this is simply a function of x, it will return valid values for unreachable x at t < 0.
	 *
	 * also, be aware that the vertical trajectory case (velocity.x = 0) is NOT handled and WILL result in division by 0
	 * @param X
	 * @return 
	 */
	virtual double YOfX(const double X) const = 0;
};

namespace TrajectoryDiscretization
{
	// an approximation of a 2d curve (trajectory in our case) within a given interval
	class IIntervalApproximation
	{
	public:
		// get the y value that this approximation yields for the given x
		virtual double GetValue(double X) const = 0;

		// returns the approximated trajectory
		virtual const FBallisticTrajectory2D* GetTrajectory() const = 0;

		// for the given X, returns the error, defined as the curve (trajectory) value minus the approximation value
		float GetError(double X) const
		{
			return GetTrajectory()->YOfX(X) - GetValue(X);
		}

		/**
		 * finds the largest deviation of the trajectory from the approximation in the interval
		 * @return 
		 */
		virtual double GetLargestApproximationError() const
		{
			return GetError(GetXOfLargestApproximationError());
		}

		virtual double GetXOfLargestApproximationError() const = 0;

		virtual double GetIntervalStart() const = 0;
		virtual double GetIntervalEnd() const = 0;

		virtual ~IIntervalApproximation() = default;
	};

	// approximate the trajectory on a given interval with a line passing through two points on the trajectory
	class FLinearIntervalApproximation : public IIntervalApproximation
	{
		float A;		// y-intercept
		float B;		// slope
		double IntervalStart;
		double IntervalEnd;
		double XOfLargestError;
		double LargestError;
		const FBallisticTrajectory2D* Trajectory;
		
	public:
		FLinearIntervalApproximation() : A(0.), B(0.), IntervalStart(0.), IntervalEnd(0.), Trajectory(nullptr),
		                                 XOfLargestError(0.), LargestError(0.)
		{
		}
		
		// create a linear approximation of the given trajectory in the given interval
		FLinearIntervalApproximation(const FBallisticTrajectory2D* Trajectory,
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

		virtual const FBallisticTrajectory2D* GetTrajectory() const override
		{
			return Trajectory;
		}
		
		virtual double GetValue(double X) const override
		{
			return A + B * X;
		}

		virtual double GetXOfLargestApproximationError() const override
		{
			return XOfLargestError;
		}

		virtual double GetLargestApproximationError() const override
		{
			return LargestError;
		}

		// subdivides this approximation into two approximation at the given X
		void Subdivide(const double X, FLinearIntervalApproximation& Left, FLinearIntervalApproximation& Right) const
		{
			Left = FLinearIntervalApproximation(Trajectory, IntervalStart, X);
			Right = FLinearIntervalApproximation(Trajectory, X, IntervalEnd);
		}

		virtual double GetIntervalEnd() const override
		{
			return IntervalEnd;
		}

		virtual double GetIntervalStart() const override
		{
			return IntervalStart;
		}

		bool operator==(const FLinearIntervalApproximation& Other) const
		{
			return this->A == Other.A && this->B == Other.B;
		}
	};

	// approximate a 2d trajectory on a given interval with a polyline
	class FPolylineIntervalApproximation : public IIntervalApproximation
	{
		TArray<FLinearIntervalApproximation> Intervals;
		const FBallisticTrajectory2D* Trajectory;
		double IntervalStart;
		double IntervalEnd;

	public:

		FPolylineIntervalApproximation(const FBallisticTrajectory2D* Trajectory,
			double IntervalStart,
			double IntervalEnd) : Trajectory(Trajectory), IntervalStart(IntervalStart), IntervalEnd(IntervalEnd)
		{
			Intervals.Add(FLinearIntervalApproximation(Trajectory, IntervalStart, IntervalEnd));
		}

		virtual const FBallisticTrajectory2D* GetTrajectory() const override
		{
			return Trajectory;
		}

		virtual double GetValue(double X) const override
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

		virtual double GetXOfLargestApproximationError() const override
		{
			return Algo::MaxElementBy(Intervals, [](const FLinearIntervalApproximation& Interval)
			{
				return Interval.GetLargestApproximationError();
			})->GetXOfLargestApproximationError();
		}

		virtual double GetIntervalEnd() const override
		{
			return IntervalEnd;
		}

		virtual double GetIntervalStart() const override
		{
			return IntervalStart;
		}

		TArray<FVector2D> GetPoints() const
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

		/**
		 * finds the polyline interval with the worst (largest) approximation error and subdivides it into two
		 */
		void Subdivide()
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

		int GetNumIntervals() const
		{
			return Intervals.Num();
		}

		/**
		 * approximates a trajectory on a given interval with a polyline
		 * @param Trajectory 
		 * @param IntervalStart 
		 * @param IntervalEnd 
		 * @param MaxIntervals 
		 * @param Tolerance 
		 * @return 
		 */
		static FPolylineIntervalApproximation ApproximateTrajectory(const FBallisticTrajectory2D* Trajectory,
		                                                            double IntervalStart,
		                                                            double IntervalEnd,
		                                                            int MaxIntervals,
		                                                            float Tolerance = 0.5f)
		{
			FPolylineIntervalApproximation Approximation(Trajectory, IntervalStart, IntervalEnd);
			while (Approximation.GetLargestApproximationError() > Tolerance && Approximation.GetNumIntervals() < MaxIntervals)
			{
				Approximation.Subdivide();
			}
			return Approximation;
		}
	};
}

/**
 * a simple 2d ballistic trajectory with no drag and an origin at (0;0)
 *
 * note that this trajectory only lives in the +x/+y or +x/-y quadrants, so in case you supply a negative x-velocity,
 * its absolute value will be used instead
 */
class FSimpleBallisticTrajectory2D : public FBallisticTrajectory2D
{
	FVector2D InitialVelocity;
	float GravityZ; // should be a negative value if gravity pulls down
	
public:
	FSimpleBallisticTrajectory2D(const FVector2D& InitialVelocity, const float GravityZ) : GravityZ(GravityZ)
	{
		checkf(!InitialVelocity.IsNearlyZero(), TEXT("Expected a non-zero velocity for trajectory"));
		checkf(GravityZ < 0., TEXT("Expected a negative number for gravity"));
		this->InitialVelocity = FVector2D(FMath::Abs(InitialVelocity.X), InitialVelocity.Y);
	}

	/**
	 * this trajectory as y (vertical position) expressed as a function of x (horizontal position)
	 *
	 * be aware that this is simply a function of x, it will return valid values for unreachable x at t < 0.
	 *
	 * also, be aware that the vertical trajectory case (velocity.x = 0) is NOT handled and WILL result in division by 0
	 * @param X
	 * @return 
	 */
	virtual double YOfX(const double X) const override
	{
		return InitialVelocity.Y / InitialVelocity.X * X + GetGravityZ() / (2 * FMath::Pow(InitialVelocity.X, 2)) *
			FMath::Pow(X, 2);
	}

	virtual bool IsVertical() const override
	{
		return FMath::IsNearlyZero(InitialVelocity.X);
	}
	
	virtual FVector2D GetInitialVelocity() const override
	{
		return InitialVelocity;
	}
	
	virtual FVector2D GetOrigin() const override
	{
		return FVector2D::ZeroVector;
	}
	
	virtual FVector2D GetPositionAtTime(float Time) const override
	{
		const double PositionX = InitialVelocity.X * Time;
		const double PositionY = InitialVelocity.Y * Time + FMath::Square(Time) / 2. * GetGravityZ();
		return FVector2D(PositionX, PositionY);
	}
	
	virtual FVector2D GetVelocityAtTime(float Time) const override
	{
		// without air resistance, horizontal velocity remains constant
		return FVector2D(InitialVelocity.X, InitialVelocity.Y + GetGravityZ() * Time);
	}
	
	virtual FVector2D GetAccelerationAtTime(float Time) const override
	{
		return FVector2D(0., GetGravityZ());
	}

	virtual bool DoesPassThrough(const FVector2D& Position, double Tolerance = 1e-08) const override
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
	
	virtual float GetTimeRequiredToReach(const FVector2D& Position) const override
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

	virtual float GetGravityZ() const override
	{
		return GravityZ;
	}

	virtual float GetTimeAtApex() const override
	{
		return InitialVelocity.Y / FMath::Abs(GravityZ);
	}

	virtual float GetLaunchAngle() const override
	{
		if (IsVertical())
		{
			return FVerticalBallisticTrajectory(InitialVelocity.Y, GravityZ).GetLaunchAngle();
		}

		return FMath::Atan(InitialVelocity.Y / InitialVelocity.X);
	}
	
	virtual bool Calibrate(const FVector2D& TargetPosition, bool PreferFastest) override
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

	virtual TArray<FVector2D> Discretize(const FVector2D& Start,
		const FVector2D& End,
		int MaxNumberOfIntervals,
		float Tolerance) const override
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
};


/**
 * a ballistic trajectory that is constrained to a (usually vertical) plane with an origin in the world
 */
class FPlanarBallisticTrajectory : public IBallisticTrajectory
{
	/**
	 * no reason to solve 3d math because the trajectory is de-facto constrained to a vertical plane and hence is 2d
	 * this helper 2d trajectory is used for computations and is in sync (velocity and gravity z) with the 3d trajectory
	 */
	TSharedPtr<FBallisticTrajectory2D> Helper2DImpl;
	
	FVector InitialVelocity;
	FVector Origin; // this trajectory in 3d space allows for arbitrary origin in the world
	float GravityZ;
	UWorld* World;

	/**
	 * @return normal of velocity's XY-projectiton
	 */
	FVector GetShotDirectionXY() const
	{
		return InitialVelocity.GetSafeNormal2D();
	}

	/**
	 * convert world location to a location in trajectory 2d's space
	 */
	FVector2D WorldPosToTrajectory2DLocalPos(const FVector& WorldPos) const
	{
		return FVector2D(
			FVector::VectorPlaneProject(WorldPos - Origin, FVector::UpVector).Length(),
			(WorldPos - Origin).Z
		);
	}

	FVector Trajectory2DLocalPosToWorldPos(const FVector2D& Trajectory2DLocalPos) const
	{
		return Origin +
			GetShotDirectionXY() * Trajectory2DLocalPos.X +
			FVector::UpVector * Trajectory2DLocalPos.Y;
	}
	
public:

	FPlanarBallisticTrajectory(
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

	static TOptional<FPlanarBallisticTrajectory> CreateChecked(UWorld* World, const FVector& Origin, const FVector& InitialVelocity, const float GravityZ)
	{
		if (!World || InitialVelocity.IsZero() || GravityZ >= 0.)
		{
			return {};
		}

		return FPlanarBallisticTrajectory(World, InitialVelocity, Origin, GravityZ);
	}

	static TOptional<FPlanarBallisticTrajectory> CreateChecked(const UObject* WorldContextObject, const FVector& Origin, const FVector& InitialVelocity)
	{
		if (!WorldContextObject || !WorldContextObject->GetWorld()) {
			return {};
		}
		return CreateChecked(WorldContextObject->GetWorld(), Origin, InitialVelocity, WorldContextObject->GetWorld()->GetGravityZ());
	}

	virtual bool IsVertical() const override
	{
		return Helper2DImpl->IsVertical();
	}

	virtual UWorld* GetWorld() const override
	{
		return World;
	}
	
	virtual FVector GetPositionAtTime(float Time) const override
	{
		const auto PosOn2DTrajectory = Helper2DImpl->GetPositionAtTime(Time);
		return Origin + GetShotDirectionXY() * PosOn2DTrajectory.X + FVector::UpVector * PosOn2DTrajectory.Y;
	}

	virtual FVector GetVelocityAtTime(float Time) const override
	{
		const auto VelocityOn2DTrajectory = Helper2DImpl->GetVelocityAtTime(Time);
		return GetShotDirectionXY() * VelocityOn2DTrajectory.X + FVector::UpVector * VelocityOn2DTrajectory.Y;
	}

	virtual FVector GetAccelerationAtTime(float Time) const override
	{
		const auto AccelerationOn2DTrajectory = Helper2DImpl->GetAccelerationAtTime(Time);
		return GetShotDirectionXY() * AccelerationOn2DTrajectory.X + FVector::UpVector * AccelerationOn2DTrajectory.Y;
	}

	virtual FVector GetInitialVelocity() const override
	{
		return InitialVelocity;
	}

	virtual FVector GetOrigin() const override
	{
		return Origin;
	}

	virtual bool DoesPassThrough(const FVector& Position, double Tolerance = 1e-08) const override
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
	
	virtual float GetTimeRequiredToReach(const FVector& Position) const override
	{
		if (!DoesPassThrough(Position))
		{
			return -1.;
		}
		return Helper2DImpl->GetTimeRequiredToReach(WorldPosToTrajectory2DLocalPos(Position));
	}

	virtual float GetLaunchAngle() const override
	{
		return Helper2DImpl->GetLaunchAngle();
	}
	
	virtual float GetTimeAtApex() const override
	{
		return Helper2DImpl->GetTimeAtApex();
	}
	
	virtual float GetGravityZ() const override
	{
		return GravityZ;
	}
	
	virtual bool Calibrate(const FVector& TargetPosition, bool PreferFastest) override
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

	virtual TArray<FVector> Discretize(const FVector& Start,
		const FVector& End,
		int MaxNumberOfIntervals,
		float Tolerance) const override
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
};