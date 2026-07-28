#!/usr/bin/env python3
"""User-session approval agent shared by Constable and mYstable."""

import argparse
import json
import os
import socket
import stat
import sys


def load_choices(path):
    if not path:
        return {}
    try:
        with open(path, encoding="utf-8") as stream:
            data = json.load(stream)
        return {key: value for key, value in data.items()
                if value in ("allow", "deny")}
    except (FileNotFoundError, OSError, ValueError):
        return {}


def save_choices(path, choices):
    if not path:
        return
    os.makedirs(os.path.dirname(os.path.abspath(path)),
                mode=0o700, exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as stream:
        json.dump(choices, stream, indent=2, sort_keys=True)
        stream.write("\n")
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)


def popup(event, policy):
    try:
        import tkinter as tk
    except ImportError:
        return "deny", False
    answer = {"choice": "deny", "remember": False}
    try:
        root = tk.Tk()
    except tk.TclError:
        return "deny", False
    root.title("Medusa permission request")
    root.resizable(False, False)
    tk.Label(root, text="Permission request", font=("", 14, "bold")).pack(
        padx=24, pady=(20, 8))
    tk.Label(root, text=event).pack(padx=24, pady=4)
    tk.Label(root, text="Policy recommendation: " + policy).pack(
        padx=24, pady=(4, 12))
    remember = tk.BooleanVar(value=False)
    tk.Checkbutton(root, text="Remember for this event",
                   variable=remember).pack(pady=4)
    buttons = tk.Frame(root)
    buttons.pack(padx=20, pady=(8, 20))

    def choose(value):
        answer["choice"] = value
        answer["remember"] = bool(remember.get())
        root.destroy()

    tk.Button(buttons, text="Deny", width=12,
              command=lambda: choose("deny")).pack(side=tk.LEFT, padx=5)
    tk.Button(buttons, text="Allow", width=12,
              command=lambda: choose("allow")).pack(side=tk.LEFT, padx=5)
    root.protocol("WM_DELETE_WINDOW", lambda: choose("deny"))
    root.attributes("-topmost", True)
    root.mainloop()
    return answer["choice"], answer["remember"]


def terminal_prompt(event, policy):
    reply = input(
        f"Medusa permission {event} (policy: {policy}) "
        "[a]llow/[d]eny/[A]lways allow/[D]always deny: "
    ).strip()
    return (("allow", reply == "A") if reply in ("a", "A")
            else ("deny", reply == "D"))


def parse_request(line):
    fields = line.rstrip("\n").split("\t")
    if (len(fields) != 5 or fields[:2] != ["REQ", "1"]
            or fields[3] not in ("allow", "deny")
            or not fields[2].isdigit()
            or not fields[4] or any(char in fields[4] for char in "\r\n\t")):
        raise ValueError("invalid approval request")
    return fields[2], fields[3], fields[4]


def serve(args):
    choices = load_choices(args.state)
    if args.clear:
        choices.clear()
        save_choices(args.state, choices)
        return
    try:
        status = os.lstat(args.socket)
        if (not stat.S_ISSOCK(status.st_mode)
                or status.st_uid != os.getuid()):
            raise RuntimeError("refusing to replace a non-socket path")
        os.unlink(args.socket)
    except FileNotFoundError:
        pass
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind(args.socket)
    os.chmod(args.socket, 0o600)
    listener.listen(16)
    try:
        while True:
            connection, _ = listener.accept()
            with connection:
                try:
                    connection.settimeout(5)
                    line = connection.makefile(
                        "r", encoding="utf-8", newline="\n").readline(512)
                    request_id, policy, event = parse_request(line)
                    if event in choices:
                        answer, remember = choices[event], True
                    else:
                        answer, remember = (
                            terminal_prompt(event, policy) if args.terminal
                            else popup(event, policy)
                        )
                    if remember:
                        choices[event] = answer
                        try:
                            save_choices(args.state, choices)
                        except OSError as error:
                            print(f"could not save remembered choice: {error}",
                                  file=sys.stderr)
                    connection.sendall(
                        f"RES\t1\t{request_id}\t{answer}\t"
                        f"{'remember' if remember else 'once'}\n".encode())
                except (OSError, ValueError, EOFError) as error:
                    print(f"approval request failed: {error}", file=sys.stderr)
    finally:
        listener.close()
        try:
            os.unlink(args.socket)
        except FileNotFoundError:
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--socket", required=True)
    parser.add_argument("--state",
                        help="0600 JSON file for remembered per-event choices")
    parser.add_argument("--terminal", action="store_true",
                        help="use a terminal prompt instead of Tk")
    parser.add_argument("--clear", action="store_true",
                        help="clear remembered choices and exit")
    serve(parser.parse_args())


if __name__ == "__main__":
    main()
