# Vexium Debugger

Debug Vexium `.vxm` programs directly inside VS Code — just like Python's debugger.

## Features

- **Breakpoints** — click the gutter in any `.vxm` file to set/clear breakpoints
- **Step Over** (`F10`) — execute the next line
- **Step Into** (`F11`) — step into function calls
- **Step Out** (`Shift+F11`) — run until the current function returns
- **Continue** (`F5`) — run until the next breakpoint
- **Variables panel** — inspect all in-scope variables while paused
- **Call stack** — see the current execution location
- **Debug Console** — program output appears live

## Requirements

- **Vexium runtime** (`vexium.exe`) must be installed.
  Download from the [Vexium releases page](https://github.com/vexium/vexium).
- VS Code 1.70 or later.

## Quick Start

1. Open a `.vxm` file.
2. Press `F5` — VS Code will ask you to select a debug environment; choose **Vexium Debug**.
3. A `launch.json` is auto-created. Press `F5` again to start debugging.

## launch.json Example

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "vexium",
      "request": "launch",
      "name": "Debug my program",
      "program": "${file}",
      "stopOnEntry": true
    }
  ]
}
```

### Configuration Options

| Field | Default | Description |
|---|---|---|
| `program` | `${file}` | Path to the `.vxm` file to debug |
| `stopOnEntry` | `true` | Pause on the first line when launching |
| `runtimePath` | _(auto)_ | Full path to `vexium.exe` if not on `PATH` |

## How It Works

This extension uses the **Debug Adapter Protocol (DAP)**. It launches
`vexium.exe debug <file>` as a subprocess and communicates over stdio.
The Vexium runtime pauses at each breakpoint and provides variable state
to the adapter, which relays it to VS Code.

## License

MIT — see [LICENSE.txt](LICENSE.txt)
