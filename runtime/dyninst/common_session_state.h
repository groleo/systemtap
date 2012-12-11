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


static int stp_session_init(void)
{
	// Reserve space for the session at the beginning of shared memory.
	void *session = _stp_shm_zalloc(sizeof(struct stp_runtime_session));
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

	return 0;
}
