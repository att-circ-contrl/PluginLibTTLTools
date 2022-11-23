#ifndef TTLTOOLSDEBUG_H_DEFINED
#define TTLTOOLSDEBUG_H_DEFINED


// Debugging switches.

// Debug code enable/disable switch.
#define LOGICWANTDEBUG 1

// Warning enable switch (does not require debug enabled).
#define LOGICWANTWARNINGS 1

// Condition bypass switch. Set this to make conditional triggers act like FIFOs.
#define LOGICDEBUG_BYPASSCONDITION 0


// Diagnostic tattle macros.

#define LOGICDEBUG_DEFAULT_DEBUGID (-1)

// Report prefix string. This defaults to "[TTLTools]  ".
#ifndef LOGICDEBUGPREFIX
#define LOGICDEBUGPREFIX "[TTLTools]  "
#endif

// Conditional code block.
#if LOGICWANTDEBUG
#define L_DEBUG(x) do { x } while(false);
#else
#define L_DEBUG(x) {}
#endif

// Debug tattle output.
// Flushing should already happen with std::endl, but force it anyways.
// NOTE - Adding a switch to prepend instance IDs if desired.
#ifdef LOGICDEBUGIDVARIABLE
#define L_PRINT(x) L_DEBUG(std::cout << LOGICDEBUGPREFIX << "(" << LOGICDEBUGIDVARIABLE << ")  " << x << std::endl << std::flush;)
#else
#define L_PRINT(x) L_DEBUG(std::cout << LOGICDEBUGPREFIX << x << std::endl << std::flush;)
#endif

// Warning tattle output.
// Flushing should already happen with std::endl, but force it anyways.
// NOTE - Adding a switch to prepend instance IDs if desired.
#define L_WARNCODE(x) do { x } while(false);
#ifdef LOGICDEBUGIDVARIABLE
#define L_WARN(x) L_WARNCODE(std::cout << LOGICDEBUGPREFIX << "(" << LOGICDEBUGIDVARIABLE << ")  " << x << std::endl << std::flush;)
#else
#define L_WARN(x) L_WARNCODE(std::cout << LOGICDEBUGPREFIX << x << std::endl << std::flush;)
#endif


#endif
