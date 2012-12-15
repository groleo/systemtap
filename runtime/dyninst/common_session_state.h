// Will be included once by runtime/common_session_state.h, which is included
// once by translate.cxx c_unparser::emit_common_header ().


// Forward declarations for things in runtime/dyninst/shm.c
static void *_stp_shm_base;
static void *_stp_shm_alloc(size_t size);


// Global state shared throughout the module
struct stp_runtime_session {
	atomic_t _session_state;

	atomic_t _error_count;
	atomic_t _skipped_count;
	atomic_t _skipped_count_lowstack;
	atomic_t _skipped_count_reentrant;
	atomic_t _skipped_count_uprobe_reg;
	atomic_t _skipped_count_uprobe_unreg;

#ifdef STP_ALIBI
	atomic_t _probe_alibi[STP_PROBE_COUNT];
#endif

#ifdef STP_TIMING
	offptr_t _probe_timing[STP_PROBE_COUNT]; // offptr<Stat>
#endif

	struct stp_globals _global;

	// NB: the context includes a number of pointers, which wouldn't be
	// kosher for shared memory, but it's ok as long as they're only set
	// and dereferenced within each separate handler invocation.
	struct context _context[]; // variably-sized to cpu count
};

static inline struct stp_runtime_session* _stp_session(void)
{
	// Since the session is always the first thing allocated, it lives
	// directly at the start of shared memory.
	// NB: We always reference the main shm pointer, rather than our own
	// local copy, because reallocations may cause it to move dynamically.
	return _stp_shm_base;
}


#define GET_SESSION_ATOMIC(name)			\
	static inline atomic_t *name(void) {		\
		return &_stp_session()->_##name;	\
	}

GET_SESSION_ATOMIC(session_state);

GET_SESSION_ATOMIC(error_count);
GET_SESSION_ATOMIC(skipped_count);
GET_SESSION_ATOMIC(skipped_count_lowstack);
GET_SESSION_ATOMIC(skipped_count_reentrant);
GET_SESSION_ATOMIC(skipped_count_uprobe_reg);
GET_SESSION_ATOMIC(skipped_count_uprobe_unreg);

#undef GET_SESSION_ATOMIC


#ifdef STP_ALIBI
static inline atomic_t *probe_alibi(size_t index)
{
	// Do some simple bounds-checking.  Translator-generated code
	// should never get this wrong, but better to be safe.
	index = clamp_t(size_t, index, 0, STP_PROBE_COUNT - 1);
	return &_stp_session()->_probe_alibi[index];
}
#endif

#ifdef STP_TIMING
static inline Stat probe_timing(size_t index)
{
	// Do some simple bounds-checking.  Translator-generated code
	// should never get this wrong, but better to be safe.
	index = clamp_t(size_t, index, 0, STP_PROBE_COUNT - 1);
	return offptr_get(&_stp_session()->_probe_timing[index]);
}
#endif


static inline struct context* stp_session_context(size_t index)
{
	// Do some simple bounds-checking.  Translator-generated code
	// should never get this wrong, but better to be safe.
	index = clamp_t(size_t, index, 0, _stp_runtime_num_contexts - 1);
	return &_stp_session()->_context[index];
}


#define _global_raw(name)	(_stp_session()->_global.name)
#define _global_type(name)	typeof(_global_raw(name))
#define _global_cast_type(name)	_global_type(name ## _typed)
#define _global_offptr_p(name)	\
	__builtin_types_compatible_p(_global_type(name), offptr_t)
#define _global_compat_p(name, value)	\
	__builtin_types_compatible_p(_global_cast_type(name), typeof(value))

// NB: The (offptr_t*) cast is unfortunately necessary for the first expression
// to pass syntax on long/string types, but __builtin_choose_expr will still
// direct them to the second expression due to the _global_offptr_p check.
#define global(name)							\
	__builtin_choose_expr(_global_offptr_p(name),			\
		((_global_cast_type(name))				\
			offptr_get((offptr_t*)&_global_raw(name))),	\
		_global_raw(name))

// NB: This is only used for pointer->offptr assignments, for stats and maps.
// Unlike global() which just returns string/long types by value, here we'd
// have to encode their separate assignment schemes, in a way that stays
// syntax-compatible with all others.  The translator can do that simpler.
// NB: Evaluate the value *before* the offptr destination is computed, in
// case the value's allocation causes shm to move.
#define global_set(name, value)	({					\
	typeof(value) __value = (value);				\
	offptr_set(&_global_raw(name),					\
		__builtin_choose_expr(_global_compat_p(name, __value),	\
			__value, (void)0));				\
	__value; })

#define global_lock(name)	(&_global_raw(name ## _lock))
#define global_lock_init(name)	\
	stp_pthread_rwlock_init_shared(global_lock(name))

#ifdef STP_TIMING
#define global_skipped(name)	(&_global_raw(name ## _lock_skip_count))
#endif


static int stp_session_init(void)
{
	size_t i;

	// Reserve space for the session at the beginning of shared memory.
	size_t session_size = sizeof(struct stp_runtime_session)
		+ sizeof(struct context) * _stp_runtime_num_contexts;
	void *session = _stp_shm_zalloc(session_size);
	if (!session)
		return -ENOMEM;

	// If we weren't the very first thing allocated, then something is wrong!
	if (session != _stp_shm_base)
		return -EINVAL;

	atomic_set(session_state(), STAP_SESSION_STARTING);

	atomic_set(error_count(), 0);
	atomic_set(skipped_count(), 0);
	atomic_set(skipped_count_lowstack(), 0);
	atomic_set(skipped_count_reentrant(), 0);
	atomic_set(skipped_count_uprobe_reg(), 0);
	atomic_set(skipped_count_uprobe_unreg(), 0);

#ifdef STP_ALIBI
	// Initialize all the alibi counters
	for (i = 0; i < STP_PROBE_COUNT; ++i)
		atomic_set(probe_alibi(i), 0);
#endif

#ifdef STP_TIMING
	// Initialize each Stat used for timing information
	for (i = 0; i < STP_PROBE_COUNT; ++i) {
		// NB: we don't check for null return here, but instead at
		// passage to probe handlers and at final printing.
		Stat st = _stp_stat_init(HIST_NONE);

		// NB: allocate first, then dereference the session after, in case
		// allocation-resizing causes the whole thing to move around.
		offptr_set(&_stp_session()->_probe_timing[i], st);
	}
#endif

	return 0;
}
