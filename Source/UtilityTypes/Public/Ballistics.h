#pragma once

#include "CoreMinimal.h"
#include "Algo/MaxElement.h"
#include "Kismet/KismetSystemLibrary.h"

#include "Ballistics.generated.h"

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
	FORCEINLINE bool HasApex() const
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
	FORCEINLINE bool IsPointingUp() const
	{
		return GetLaunchAngle() > 0.;
	}

	/**
	 * whether this trajectory has a negative launch angle (points down)
	 */
	FORCEINLINE bool IsPointingDown() const
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
	/**
	 * sweeps multiple line traces between successive points in an array
	 * @param Points 
	 * @param HitSegment		the index of the segment in trajectory's linearization where the hit occurred (-1 if no hit)
	 * @param OutHit 
	 * @param Rot 
	 * @param CollisionShape 
	 * @param TraceChannel 
	 * @param Params 
	 * @param ResponseParams 
	 * @param ObjectParams 
	 * @return 
	 */
	bool GeomSweepSingleIterative(const TArray<FVector>& Points,
	                              FHitResult& OutHit,
	                              int& HitSegment,
	                              const FQuat& Rot,
	                              const FCollisionShape& CollisionShape,
	                              ECollisionChannel TraceChannel,
	                              const FCollisionQueryParams& Params,
	                              const FCollisionResponseParams& ResponseParams,
	                              const FCollisionObjectQueryParams& ObjectParams =
		                              FCollisionObjectQueryParams::DefaultObjectQueryParam) const;
	
public:
	// get the world this trajectory exists in
	virtual UWorld* GetWorld() const = 0;
	
	/**
	 * sweep a shape along the trajectory
	 *
	 * this will actually perform multiple linear sweeps, constrained by MaxIterations
	 * @param OutHit			the first blocking hit
	 * @param Points			the array of points which resulted after trajectory's discretization and between which linear sweeps were performed
	 * @param HitSegment		the index of the segment in trajectory's linearization where the hit occurred (-1 if no hit)
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
	                          TArray<FVector>& Points,
	                          int& HitSegment,
	                          const FVector& Start,
	                          const FVector& End,
	                          ECollisionChannel TraceChannel,
	                          const FCollisionShape& CollisionShape,
	                          const FQuat& Rot,
	                          const int MaxIterations,
	                          const float Tolerance,
	                          const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
	                          const FCollisionResponseParams& ResponseParams =
		                          FCollisionResponseParams::DefaultResponseParam) const
	{
		Points = Discretize(Start, End, MaxIterations, Tolerance);
		return GeomSweepSingleIterative(Points, OutHit, HitSegment, Rot, CollisionShape,
		                                TraceChannel, Params, ResponseParams);
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

	virtual bool Calibrate(const double& TargetPosition, bool PreferFastest) override;

	/**
	 * returns the shortest time required to reach a position. if the position is not reachable with given velocity, returns a negative value
	 * @param Position 
	 * @return 
	 */
	virtual float GetTimeRequiredToReach(const double& Position) const override;
	
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
		FLinearIntervalApproximation(const FBallisticTrajectory2D* Trajectory, double IntervalStart, double IntervalEnd);

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

		virtual double GetValue(double X) const override;

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

		TArray<FVector2D> GetPoints() const;

		/**
		 * finds the polyline interval with the worst (largest) approximation error and subdivides it into two
		 */
		void Subdivide();

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
		                                                            float Tolerance = 0.5f);
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
		return FVector2D(InitialVelocity.X * Time,
			InitialVelocity.Y * Time + FMath::Square(Time) / 2. * GetGravityZ());
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

	virtual bool DoesPassThrough(const FVector2D& Position, double Tolerance = 1e-08) const override;
	
	virtual float GetTimeRequiredToReach(const FVector2D& Position) const override;

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
	
	virtual bool Calibrate(const FVector2D& TargetPosition, bool PreferFastest) override;

	virtual TArray<FVector2D> Discretize(const FVector2D& Start,
		const FVector2D& End,
		int MaxNumberOfIntervals,
		float Tolerance) const override;
};


/**
 * a ballistic trajectory that is constrained to a (usually vertical) plane with an origin in the world
 */
class UTILITYTYPES_API FPlanarBallisticTrajectory : public IBallisticTrajectory
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
	FPlanarBallisticTrajectory(UWorld* World, const FVector& InitialVelocity, const FVector& Origin,
	                           const float GravityZ
	);

	static TOptional<FPlanarBallisticTrajectory> CreateChecked(UWorld* World, const FVector& Origin, const FVector& InitialVelocity, const float GravityZ);
	static TOptional<FPlanarBallisticTrajectory> CreateChecked(const UObject* WorldContextObject, const FVector& Origin, const FVector& InitialVelocity);

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

	virtual bool DoesPassThrough(const FVector& Position, double Tolerance = 1e-08) const override;
	
	virtual float GetTimeRequiredToReach(const FVector& Position) const override;

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
	
	virtual bool Calibrate(const FVector& TargetPosition, bool PreferFastest) override;

	virtual TArray<FVector> Discretize(const FVector& Start, const FVector& End, int MaxNumberOfIntervals,
	                                   float Tolerance) const override;
};

// the blueprint-facing layer
USTRUCT(BlueprintType)
struct UTILITYTYPES_API FBallisticTrajectory
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FVector InitialVelocity;

	UPROPERTY(BlueprintReadOnly)
	FVector Origin;

	UPROPERTY(BlueprintReadOnly)
	float GravityZ;
};

UCLASS()
class UTILITYTYPES_API UBallisticTrajectoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintPure, meta=(WorldContext="WorldContextObject"))
	static FBallisticTrajectory FindPassThroughTrajectory(
		bool& Success,
		const UObject* WorldContextObject,
		const FVector& Origin,
		const float Velocity,
		const FVector& PassThroughLocation,
		const bool PreferShortArc = true);

	/**
	 * 
	 * @param WorldContextObject 
	 * @param Trajectory				The trajectory along which to perform the sweep.
	 * @param EndTime					Time along the trajectory when to end the trace. 
	 * @param Tolerance					For linearization of the trajectory, the acceptable deviation of the approximation from the trajectory.
	 * @param MaxLinearizeIntervals		For linearization of the trajectory, the maximum number of discrete intervals in which the trajectory will be divided.
	 * @param Radius					Radius of the sphere to sweep.
	 * @param TraceChannel 
	 * @param bTraceComplex				True to test against complex collision, false to test against simplified collision.
	 * @param ActorsToIgnore 
	 * @param DrawDebugType 
	 * @param OutHit					Properties of the trace hit.
	 * @param bIgnoreSelf 
	 * @param TraceColor 
	 * @param TraceHitColor 
	 * @param DrawTime 
	 * @return							Tru eif there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(Tolerance="1.f", MaxLinearizeIntervals="10", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "Sphere Trace On Trajectory", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", Keywords="sweep"))
	static bool SphereTraceSingleOnTrajectory(const UObject* WorldContextObject,
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
		FLinearColor TraceColor = FLinearColor::Red,
		FLinearColor TraceHitColor = FLinearColor::Green,
		float DrawTime = 5.0f);
};
