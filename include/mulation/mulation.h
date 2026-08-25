#ifndef MULATION_H
#define MULATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile-time instrumentation calls this to choose original vs mutant.
 * The runner sets MULATION_MUTANT=<id> in the environment; unset means
 * no mutant is active (baseline / production-equivalent behavior).
 *
 * Also records coverage when MULATION_HITLOG is set (baseline pass).
 */
int mulation_active(unsigned id);

#ifdef __cplusplus
}
#endif

#endif /* MULATION_H */
