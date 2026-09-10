#pragma once

namespace MayaFlux::Kinesis {

/**
 * @enum LatticeValueClass
 * @brief What the values sampled over a lattice mean geometrically.
 *
 * A description of the data, not of any consumer. A fog volume's values
 * are densities: nonnegative, zero outside the medium, meaningful only in
 * aggregate along a ray. A level set's are signed distances whose zero
 * crossing is a surface and whose gradient is a normal. A staggered
 * field's components are sampled on cell faces rather than at the centre,
 * which is a placement property rather than a meaning one, but it travels
 * in this enumeration because every interchange format that carries the
 * distinction carries it here.
 *
 * Unknown is the honest default. A field left Unknown is a plain array of
 * numbers over a lattice, which most fields are, and nothing downstream
 * should infer otherwise from its name.
 *
 * The enumerators correspond one to one with OpenVDB's GridClass and with
 * the tokens USD's UsdVol schema accepts for fieldClass, so an interchange
 * writer maps them without a lookup table and without loss.
 */
enum class LatticeValueClass : uint8_t {
    Unknown, ///< Plain numeric values with no further structure claimed.
    LevelSet, ///< Signed distance to a surface at the zero crossing.
    FogVolume, ///< Density of a participating medium, zero outside it.
    Staggered, ///< Vector components sampled on cell faces, not centres.
};

/**
 * @enum VectorVariance
 * @brief How a vector quantity's components behave under a change of frame.
 *
 * The distinction is differential-geometric and independent of storage: it
 * says whether the numbers stored are attached to the lattice's coordinate
 * basis or to the world, and therefore what must happen to them when the
 * lattice is scaled, rotated or resampled.
 *
 * A velocity is ContravariantRelative: halve the cell size and the stored
 * components halve with it, because a velocity is a displacement per unit
 * time expressed in lattice units. A surface normal is CovariantNormalize:
 * it transforms by the inverse transpose and is renormalized after. A
 * triple of unrelated scalars carried together for convenience, a colour
 * for instance, is Invariant: nothing happens to it under any change of
 * frame.
 *
 * Invariant is the default because assuming a vector is a velocity when it
 * is not silently corrupts it the first time anything rescales the lattice,
 * whereas the reverse mistake leaves values untouched and visibly wrong.
 *
 * Meaningless for scalar quantities. Carried alongside them regardless
 * rather than split into a separate optional, since the pairing is what
 * every consumer wants and a scalar's variance is simply ignored.
 *
 * The enumerators correspond one to one with OpenVDB's VecType.
 */
enum class VectorVariance : uint8_t {
    Invariant, ///< Unaffected by any change of frame.
    Covariant, ///< Transforms by the inverse transpose.
    CovariantNormalize, ///< Inverse transpose, then renormalized.
    ContravariantRelative, ///< Transforms by the frame itself; velocities.
    ContravariantAbsolute, ///< Contravariant, treated as a world-space position.
};

/**
 * @struct LatticeSemantics
 * @brief Interpretation attached to one named quantity sampled over a lattice.
 *
 * Carries no data and no lattice. It says what a separately stored array of
 * values means, so that a consumer which never saw the code that produced
 * them can resample, render or write them correctly.
 *
 * Pure description with no behaviour, which is why it lives in Kinesis
 * rather than in Buffers or IO. A GPU-resident simulation attaches one to
 * each of its stored quantities at declaration; an interchange writer reads
 * the same struct and maps it to whatever its format calls these two ideas.
 * Neither side owns it and neither needs its own copy.
 *
 * Both members default to their least presumptuous value, so a caller that
 * has nothing to say says nothing:
 *
 * @code
 * vol->declare_scalar("density", { .value_class = LatticeValueClass::FogVolume });
 * vol->declare_vector("velocity", { .variance = VectorVariance::ContravariantRelative });
 * vol->declare_scratch("divergence");
 * @endcode
 */
struct LatticeSemantics {
    LatticeValueClass value_class = LatticeValueClass::Unknown; ///< Geometric meaning of the values.
    VectorVariance variance = VectorVariance::Invariant; ///< Vector quantities only.
};

} // namespace MayaFlux::Kinesis
