#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdatomic.h>

void spinlock_acquire( atomic_flag * lock );
void spinlock_release( atomic_flag * lock );

#endif