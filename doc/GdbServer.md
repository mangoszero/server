# GDB-Server Debug Endpoint

The GDB-server lets a debugger — or an AI agent — attach to a **running**
`mangosd` process and drive it directly: read process memory, inspect live game
state, and run server/GM commands, all over a single network endpoint. It is
built on the GDB Remote Serial Protocol and integrated into the mangosd world
loop and ACE networking.

It has two layers:

1. **Native layer** — a real GDB **Remote Serial Protocol (RSP)** stub. Any
   debugger that speaks RSP can attach: GDB, LLDB, IDA Pro (its built-in
   "Remote GDB debugger" backend), radare2/rizin, Binary Ninja.
2. **Semantic layer** — mangos-specific introspection/control delivered over
   GDB's standard `qRcmd` ("monitor") packet as `mangos <verb>` commands. The
   same verbs are also exposed on a plain-text TCP bridge for AI agents and for
   debuggers that do not speak RSP (WinDbg, CDB, x64dbg).

## Enabling

In `mangosd.conf` (see the CONSOLE / REMOTE ACCESS section):

```
GdbServer.Enable        = 1
GdbServer.IP            = 127.0.0.1   # never expose publicly
GdbServer.Port          = 2345        # RSP port
GdbServer.MonitorPort   = 2346        # plain-text bridge (0 to disable)
GdbServer.AllowMemWrite = 0           # allow RSP 'M' writes (dangerous)
```

> **Security:** anyone who can reach these ports gets full process-memory read
> and full server-command access. Keep the bind address on `127.0.0.1`. The
> subsystem is disabled by default. Memory writes (`M`) and are off by default.

At boot the server runs a self-test and logs `[gdb-monitor-selftest] PASS`,
then (when enabled) `GdbServer Thread started (RSP on 127.0.0.1:2345 ...)`.

## Attaching

### GDB / LLDB / IDA (RSP)

```
# GDB
gdb -ex "target remote 127.0.0.1:2345"

# LLDB
lldb -o "gdb-remote 127.0.0.1:2345"

# IDA Pro: Debugger -> Attach -> Remote GDB debugger,
#          host 127.0.0.1, port 2345
```

Once attached, the semantic layer is reached with `monitor`:

```
(gdb) monitor mangos help
(gdb) monitor mangos status
(gdb) monitor mangos players
(gdb) monitor mangos config WorldServerPort
(gdb) monitor mangos cmd .server info
(gdb) x/16xb 0x<addr>          # read live process memory
```

Pressing **Ctrl-C** in gdb interrupts the target: the world tick pauses and the
process enters a cooperative stop loop until you `continue`. The server still
honours shutdown signals while paused, so it can always be stopped.

### AI agents / WinDbg / CDB / x64dbg (plain-text bridge)

Non-RSP tools attach to the `mangosd` process natively (by PID) for low-level
work, and use the plain-text monitor bridge for the semantic layer:

```
# any raw TCP client / AI agent
$ nc 127.0.0.1 2346
mangos help
mangos tick
mangos players
mangos cmd .save all
```

Each line is a `mangos <verb>` command; the text reply comes straight back.

## Monitor verbs

| Verb | Description |
|------|-------------|
| `mangos help` | List verbs |
| `mangos status` | World summary: motd, uptime, sessions, world tick |
| `mangos players` (`ps`) | List online sessions/players |
| `mangos tick` | World loop counter + uptime |
| `mangos session <accId>` | Session detail for an account id |
| `mangos config <key>` | Read a `mangosd.conf` value |
| `mangos cmd <command>` | Run any server/GM command (e.g. `.server info`, `.account onlinelist`) |
| `mangos break ...` | Arm/list/clear game-level breakpoints (see below) |
| `mangos dump` | Backtrace of the world thread |

The `cmd` verb bridges to the existing `ChatCommand` system at `SEC_CONSOLE`
level, so the entire console/GM command surface is drivable over the debug
channel.

## Game-level breakpoints

Beyond the native protocol, the server can pause itself at semantically
meaningful points and hand control to the attached debugger. When a breakpoint
fires (and a debugger is attached), the world thread stops **inline at the call
site**, captures the live registers, and waits — so `bt` in gdb shows the real
call stack and `monitor mangos ...` reads quiescent game state. `continue`
resumes the tick.

```
(gdb) monitor mangos break opcode 0x12E    # pause when this opcode arrives
(gdb) monitor mangos break map 0           # pause when a player enters map 0
(gdb) monitor mangos break list
(gdb) monitor mangos break del opcode:0x12E
(gdb) monitor mangos break clear
```

Breakpoints only fire while a debugger is attached, so an armed-but-unattended
breakpoint never stalls the server. Current call sites: received-opcode dispatch
and player map-entry; more sites are added with a single `GDB_BREAK_*` macro.

## Capabilities and limits

**Implemented:**
- RSP transport over TCP; `qSupported`, `?`, `g`/`G`, `m`/`M` (guarded
  process-memory read/write), `H`, `c`/`s`/`D`/`k`, `vCont`, `qRcmd` monitor.
- The `mangos` semantic verbs including the `cmd` bridge; the plain-text bridge.
- Cooperative world-tick stop on Ctrl-C.
- **Live register capture** (`g`): at a stop the world thread's real registers
  are captured (Linux `getcontext`, Windows `RtlCaptureContext` on x86_64), so
  gdb can `bt` through the actual call stack via the `m` memory packets.
- **Game-level breakpoints** on opcodes, map-entry, and named labels.
- `mangos dump` backtrace.

**Limits:**
- `g`/`G` cover the x86_64 integer register file; FPU/SSE are reported as zero,
  and non-x86_64 builds report zeroed registers (memory + monitor still work).
- Native instruction breakpoints (`Z`/`z`) and hardware single-step are not
  implemented — by design. Driving `int3` patching or self-set debug registers
  in a live, multi-threaded server is unsafe; the game-level breakpoints above
  are the supported, cooperative equivalent.
- One debugger connection at a time.
- State seen during a stop is *world-tick-consistent*, not globally race-free:
  the network, database and map-update threads keep running.

**Possible future work:** symbol-resolved backtraces on Windows (via the
existing crash-report tooling) and additional `GDB_BREAK_*` call sites.
