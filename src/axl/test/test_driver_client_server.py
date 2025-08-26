#!/usr/bin/env python3

import sys
import subprocess as sp
import os
import pathlib
import time
import signal


def wait_for_completion(procname, proc, wait_time):
    try:
        outs, err = proc.communicate(timeout=wait_time)
    except:
        proc.terminate()
        outs, err = proc.communicate()

    print("{} Return Code: {}".format(procname, proc.returncode))
    if proc.returncode < 0:
        print(f"Process killed by signal: {signal.strsignal(-proc.returncode)}")
    print("stdout:\n{}".format(outs.decode("utf-8")))
    if isinstance(err, bytes):
        print("stderr:\n{}".format(err.decode("utf-8")))

    return proc.returncode, outs, err


def main():
    errors = 0
    # Launch the server then the client
    port = "2000"
    test_env = {
        "AXL_DEBUG": "44",
        "AXL_SERVICE_HOST": "localhost",
        "AXL_SERVICE_PORT": port,
    }
    test_file = pathlib.Path(__file__).absolute().parent / "./test_client_server"
    server = sp.Popen(
        [test_file, "--server", port],
        env=dict(os.environ, **test_env),
        stdout=sp.PIPE,
        stderr=sp.PIPE,
    )
    time.sleep(2)  # Give server a chance to start

    clients = []
    for client_message in ("foo bar baz", "hello hola bonjour buongiorno", "test test test", "a b c"):
        clients.append(sp.Popen(
            [test_file, "--client", port],
            env=dict(os.environ, AXL_SOCKET_MESSAGE=client_message, **test_env),
            stdout=sp.PIPE,
            stderr=sp.PIPE,
        ))

    client_ecode = max(abs(wait_for_completion("axl_client", client, 30)[0]) for client in clients)
    server_ecode, server_out, server_err = wait_for_completion("axl_server", server, 2)

    sys.exit(max(abs(server_ecode), client_ecode))


if __name__ == "__main__":
    main()
