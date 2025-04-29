# Kernel Scheduling Simulator

A simulation for a mini operating system kernel implementing process scheduling algorithms (RR, HPF, SRTN).

## Build

```bash
make
```

## Usage

```bash
./os-sim -s <scheduling-algorithm> -f <processes-text-file> [-q <quantum>]
```

- `<scheduling-algorithm>`: `rr`, `hpf`, or `srtn`
- `<processes-text-file>`: Path to the process list file (e.g., `processes.txt`)
- `-q <quantum>`: (Optional) Quantum for RR scheduling

### Example

```bash
./os-sim -s srtn -f processes.txt
```

## Files

- `os-sim`: Main kernel simulator executable
- `process`: Simulated process executable
- `processes.txt`: Input file with process definitions

## Notes

- The process generator spawns processes at their arrival times and sends them to the scheduler.
- The scheduler manages process execution according to the selected algorithm.
- See `scheduler.log` and `scheduler.perf` for logs and statistics after running.

---