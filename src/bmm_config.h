#ifndef BMM_CONFIG_H
#define BMM_CONFIG_H

/* Platform / build configuration for B-- */

#ifndef BMM_MAX_FRAMES
#define BMM_MAX_FRAMES 1000
#endif

#ifndef BMM_STACK_SIZE
#define BMM_STACK_SIZE (1 << 16)
#endif

#ifndef BMM_HEAP_INIT
#define BMM_HEAP_INIT (1 << 20)
#endif

#ifndef BMM_GC_THRESHOLD
#define BMM_GC_THRESHOLD 1024
#endif

#ifndef BMM_REPL
#define BMM_REPL 1
#endif

#ifndef BMM_MAX_STEPS_DEFAULT
#define BMM_MAX_STEPS_DEFAULT 0 /* 0 = unlimited */
#endif

#ifndef BMM_GC_STRESS
#define BMM_GC_STRESS 0
#endif

#endif
