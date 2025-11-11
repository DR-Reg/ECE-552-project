#ifndef __SP_MPU_HH__
#define __SP_MPU_HH__

/*- Sparse Matrix Processing Unit *-/
 *
 * mac_t should be a mac_t_p<b>
 *
 * Shape specified a priori: NxN
 *
 * Note that although it is not explicitly noted,
 * this unit performs HSA-style dataflow.
 *
 * This essentially performs a 2:1 compression
 * at the element (not block) along the x-axis
 * of the weight matrix (which should have been
 * pruned to satisfy the condition that out of
 * four contiguous elements, two are zero.
 *
 * We still have NxN grid (=> no area savings).
 * Weights are still stationary in each MAC,
 * and each MAC stores both the weight and weight index.
 * 
 * Partial sums flow left to right instead of top-bottom.
 * Activations flow top to bottom. Moreover, activations
 * carry activation index (to check hazard)
 *
 * Between each MAC we check if Wval = 0.
 * - Wval <> 0 : latch as normal
 * - Wval  = 0 : skip (i.e. gate the MAC)
 * Wval = 0 is NOT an actual check, but a flag,
 * this must be a pre-computed metadata, and during
 * loading stage, we skip even loading the value,
 * to save EMA bandwidth AND we can effectively
 * clock the design to twice the speed w/o hitting
 * thermal constraints.
 *
 * So no hazards  ??? 
 *
 *
 * Actually, STPU is a bit more complicated than above
 * (worked out the details on paper...), and although
 * SpVPU is very doable (see SpVpu.hh -- TODO), SpMpu,
 * though not impossible, is not worth spending time
 * on... (unless I have time at the very end)
 * Instead, just do SpVpu and SpHSA, where SpHSA has
 * an unchanged MMM mode, and MVM mode equivalent to SpVpu 
 */
#endif
