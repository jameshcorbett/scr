#!/bin/bash
#
#  Test runner script meant to be executed inside of a docker container
#
#  Usage: checks_run.sh [OPTIONS...]
#
#  Where OPTIONS are passed directly to `cmake``
#
#  The script is otherwise influenced by the following environment variables:
#
#  JOBS=N        Argument for make's -j option, default=2
#  PROJECT       Flux project, e.g. flux-core
#  COVERAGE      Run with --enable-code-coverage, `make check-code-coverage`
#  TEST_INSTALL  Run `make check` against installed flux-core
#  CPPCHECK      Run cppcheck if set to "t"
#  DISTCHECK     Run `make distcheck` if set
#  RECHECK       Run `make recheck` if `make check` fails the first time
#  UNIT_TEST_ONLY Only run `make check` under ./src
#  QUICK_CHECK   Run only `make TESTS=` and a simple test
#  PRELOAD       Set as LD_PRELOAD for make and tests
#  chain_lint    Run sharness with --chain-lint if chain_lint=t
#  SYSTEM        Run only the system sharness tests
#
#  And, obviously, some crucial variables that configure itself cares about:
#
#  CC, CXX, LDFLAGS, CFLAGS, etc.
#


#  Ensure uname -m reports 32bit architecture if platform was specified
#   as 386:
#
case $PLATFORM in *386)
  unset PLATFORM
  echo "Rexecuting under linux32 personality"
  exec setarch i386 $0 "$@"
  ;;
esac

# source check_group and check_time functions:
. src/test/checks-lib.sh

ARGS="$@"
JOBS=${JOBS:-2}
MAKECMDS="make -j ${JOBS} install"
CHECKCMDS="ctest"


# Force git to update the shallow clone and include tags so git-describe works
checks_group "git fetch tags" "git fetch --unshallow --tags" \
 git fetch --unshallow --tags || true

checks_group_start "build setup"
ulimit -c unlimited

source /etc/profile.d/modules.sh
module load mpi


POSTCHECKCMDS=":"
# Enable coverage for $CC-coverage build
# We can't use distcheck here, it doesn't play well with coverage testing:

if test -n "$PRELOAD" ; then
  CHECKCMDS="/usr/bin/env 'LD_PRELOAD=$PRELOAD' ${CHECKCMDS}"
fi

if test -n "$UNIT_TEST_ONLY"; then
  CHECKCMDS="(cd src && $CHECKCMDS)"
fi

checks_group_end # Setup

WORKDIR=$(pwd)
if test -n "$BUILD_DIR" ; then
  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"
  rm -f CMakeCache.txt
fi

checks_group "cmake ${ARGS}"  cmake ${ARGS} \
	|| checks_die "cmake failed" cat config.log
checks_group "make clean..." make clean

if test "$DISTCHECK" != "t"; then
  checks_group "${MAKECMDS}" "${MAKECMDS}" \
	|| checks_die "${MAKECMDS} failed"
fi
checks_group "${CHECKCMDS}" "${CHECKCMDS}" && \
	checks_group "${POSTCHECKCMDS}" "${POSTCHECKCMDS}"
RC=$?

if test "$RECHECK" = "t" -a $RC -ne 0; then
  #
  # `make recheck` is not recursive, only perform it if at least some tests
  #   under ./t were run (and presumably failed)
  #
  if test -s t/t0000-sharness.trs; then
    printf "::warning::make check failed, trying recheck in ./t\n"
		(cd t ; checks_group "make recheck" ${MAKE} -j ${JOBS} recheck) && \
			checks_group "${POSTCHECKCMDS}" "${POSTCHECKCMDS}"
    RC=$?
   else
      printf "::warning::recheck requested but no tests in ./t were run\n"
   fi
fi

exit $RC
