// Will be included once by runtime/common_session_state.h, which is included
// once by translate.cxx c_unparser::emit_common_header ().

#define DEFINE_SESSION_ATOMIC(name, init)		\
	static atomic_t g_##name = ATOMIC_INIT (init);	\
	static inline atomic_t *name(void) {		\
		return &g_##name;			\
	}

DEFINE_SESSION_ATOMIC(session_state, STAP_SESSION_STARTING);

DEFINE_SESSION_ATOMIC(error_count, 0);
DEFINE_SESSION_ATOMIC(skipped_count, 0);
DEFINE_SESSION_ATOMIC(skipped_count_lowstack, 0);
DEFINE_SESSION_ATOMIC(skipped_count_reentrant, 0);
DEFINE_SESSION_ATOMIC(skipped_count_uprobe_reg, 0);
DEFINE_SESSION_ATOMIC(skipped_count_uprobe_unreg, 0);

#undef DEFINE_SESSION_ATOMIC


static int stp_session_init(void)
{
	// All the above atomics have static initialization.
	// Nothing else to do...
	return 0;
}
