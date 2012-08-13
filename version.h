#ifndef VERSION_H
#define VERSION_H

/* If we're building from a git repo, always include that commit information.
 * Otherwise, only bother using the "non-git sources" if not configured with a
 * nice --with-extra-version.
 */
#ifdef GIT_REPO
#define STAP_EXTENDED_VERSION \
  (STAP_EXTRA_VERSION[0] ? GIT_MESSAGE ", " STAP_EXTRA_VERSION : GIT_MESSAGE)
#else
#define STAP_EXTENDED_VERSION \
  (STAP_EXTRA_VERSION[0] ? STAP_EXTRA_VERSION : GIT_MESSAGE)
#endif

#endif /* VERSION_H */
