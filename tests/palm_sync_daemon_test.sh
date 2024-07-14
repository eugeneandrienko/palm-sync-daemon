#!/usr/bin/env bash

TEST_ORG=$(mktemp /tmp/test.XXXXXX)
function cleanup()
{
    rm -f "$TEST_ORG"
}
trap cleanup EXIT

export PALM_SYNC_NOTES_ORG="$TEST_ORG"
export PALM_SYNC_TODO_ORG="$TEST_ORG"

LOCK_FILE="/tmp/palm-sync-daemon.pid"

function pidof_palm_sync_daemon()
{
    ps xo 'pid,command' | grep -v awk | grep -v '.sh' | \
        awk '/palm_sync_daemon_test/{print $1}'
}

function pidof_palm_sync_daemon_qty()
{
    ps xo 'pid,command' | grep -v awk | grep -v '.sh' | \
        awk '/palm_sync_daemon_test/{print $1}' | wc -l
}

# Test locking files
./palm_sync_daemon_test
sleep 2
if [ ! -f "$LOCK_FILE" ]; then
    echo "Failed test! Lock file not created"
    exit 1
fi
./palm_sync_daemon_test -f
if [ "$(pidof_palm_sync_daemon | wc -l)" -ne "1" ]; then
    echo "Failed test! Lock file ingored"
    pkill -f 'palm_sync_daemon_test -f'
    pidof_palm_sync_daemon | xargs kill
    exit 1
fi
./palm_sync_daemon_test -f 2>&1 | grep -q '.\{1,\}Lock file owned by process with PID [0-9]\{1,\}'
if [ "$?" -ne "0" ]; then
    echo "Failed test! Log message about lock file not printed"
    pidof_palm_sync_daemon | xargs kill
    exit 1
fi

# Test SIGINT handler
pidof_palm_sync_daemon | xargs kill -s INT
if [ -f "$LOCK_FILE" ]; then
    echo "Failed test. Lock file still exists after SIGINT"
    exit 1
fi
if [ "$(pidof_palm_sync_daemon | wc -l)" -ne "0" ]; then
    echo "Failed test. Process still running after SIGINT"
    exit 1
fi
./palm_sync_daemon_test

# Test SIGQUIT handler
pidof_palm_sync_daemon | xargs kill -s QUIT
if [ -f "$LOCK_FILE" ]; then
    echo "Failed test. Lock file still exists after SIGQUIT"
    exit 1
fi
if [ "$(pidof_palm_sync_daemon | wc -l)" -ne "0" ]; then
    echo "Failed test. Process still running after SIGQUIT"
    exit 1
fi
./palm_sync_daemon_test

# Test SIGTERM handler
pidof_palm_sync_daemon | xargs kill
if [ -f "$LOCK_FILE" ]; then
    echo "Failed test. Lock file still exists after SIGTERM"
    exit 1
fi
if [ "$(pidof_palm_sync_daemon | wc -l)" -ne "0" ]; then
    echo "Failed test. Process still running after SIGTERM"
    exit 1
fi

rm -f "$TEST_ORG"
