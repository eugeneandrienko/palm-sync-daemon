#!/usr/bin/env bash

export PALM_SYNC_NOTES_ORG="./tests/test.org"
export PALM_SYNC_TODO_ORG="./tests/test.org"

LOCK_FILE="/tmp/palm-sync-daemon.pid"

# Test locking files
./palm_sync_daemon_test
if [ ! -f "$LOCK_FILE" ]; then
    echo "Failed test! Lock file not created"
    exit 1
fi
./palm_sync_daemon_test -f
if [ "$(pidof palm_sync_daemon_test | wc -l)" -ne "1" ]; then
    echo "Failed test! Lock file ingored"
    pkill -f 'palm_sync_daemon_test -f'
    pidof palm_sync_daemon_test | xargs kill
    exit 1
fi
./palm_sync_daemon_test -f 2>&1 | grep -q '.\{1,\}Lock file owned by process with PID [0-9]\{1,\}'
if [ "$?" -ne "0" ]; then
    echo "Failed test! Log message about lock file not printed"
    pidof palm_sync_daemon_test | xargs kill
    exit 1
fi

# Test SIGINT handler
pidof palm_sync_daemon_test | xargs kill -sINT
if [ -f "$LOCK_FILE" ]; then
    echo "Failed test. Lock file still exists after SIGINT"
    exit 1
fi
if [ "$(pidof palm_sync_daemon_test | wc -l)" -ne "0" ]; then
    echo "Failed test. Process still running after SIGINT"
    exit 1
fi
./palm_sync_daemon_test

# Test SIGQUIT handler
pidof palm_sync_daemon_test | xargs kill -sQUIT
if [ -f "$LOCK_FILE" ]; then
    echo "Failed test. Lock file still exists after SIGQUIT"
    exit 1
fi
if [ "$(pidof palm_sync_daemon_test | wc -l)" -ne "0" ]; then
    echo "Failed test. Process still running after SIGQUIT"
    exit 1
fi
./palm_sync_daemon_test

# Test SIGTERM handler
pidof palm_sync_daemon_test | xargs kill
if [ -f "$LOCK_FILE" ]; then
    echo "Failed test. Lock file still exists after SIGTERM"
    exit 1
fi
if [ "$(pidof palm_sync_daemon_test | wc -l)" -ne "0" ]; then
    echo "Failed test. Process still running after SIGTERM"
    exit 1
fi
