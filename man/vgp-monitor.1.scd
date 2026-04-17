vgp-monitor(1)

# NAME

vgp-monitor - F-16 engine-page style system monitor

# SYNOPSIS

*vgp-monitor*

# DESCRIPTION

Real-time system monitor. Displays CPU / MEM / GPU / SWAP as vertical
tape-scale gauges with a caret pointer and bracketed digital readout.
Per-core CPU utilization is shown as a bracketed-readout grid with a
pointer-marked sub-bar. Additional fields cover HOST, KERNEL, UPTIME,
LOAD, RAM, SWAP, NET, DISK.

Samples _/proc/stat_, _/proc/meminfo_, _/proc/net/dev_, _/proc/diskstats_,
_/proc/loadavg_, _/proc/uptime_, and _/sys/class/drm/card*/device/gpu_busy_percent_
once per second.

# SEE ALSO

*vgp*(1)
