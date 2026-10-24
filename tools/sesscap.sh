#!/system/bin/sh
# Passive session collector for AnyPS5 / Mewgenics (PPSA37318). Runs as root, never starts or taps
# the game: it waits for the user's own launch and records one folder per session under
# /data/local/tmp/sesscap (the newest 6 are kept):
#   meta.txt     start time, env.txt, driver library checksum, battery and temperatures
#   fps.txt      the game's [fps] lines, appended every 60 s
#   hub/         first hub sighting + 15 s (on screen): frame dump, per-thread CPU, fps, device, 10 s simpleperf
#   combat/      first combat sighting + 20 s (once the game is on screen): frame dump, per-thread CPU, 10 s simpleperf
#                (call graph, whole process), battery and temperatures
#   later-HHMM/  5 min after launch, then every 20 min on screen: per-thread CPU, fps, 10 s simpleperf
#   memory.txt   (in hub/, combat/, later-*/) smaps by mapping, swap, kgsl GPU memory, meminfo
#   perf.map     box64 JIT symbols of the session (with BOX64_DYNAREC_PERFMAP=1, TMPDIR=<game dir>)
#   shots/       a screen capture every 60 s while the game is on screen (at most 40)
#   end.txt      on exit: duration, counted warnings/errors, crash buffer tail; log.txt the last 3 MiB
# Hub and combat are told from the driver log: the hub's 512x256 cat targets and the combat's
# 1280x720 sprite layer are created as storage textures there (Agc path) or logged as "[sdlgpu] target"
# by the native Vulkan SDL_GPU backend (APS5_SDLGPU_VULKAN=1), which also writes [fps] and the dumps. Needs APS5_DUMP_TRIGGER in env.txt
# for the frame dumps (a file test per present, nothing else).
echo -1000 > /proc/$$/oom_score_adj
G=/data/data/org.anyps5.android/files/games/PPSA37318
ROOT=/data/local/tmp/sesscap
LOG=$G/box64.log
mkdir -p $ROOT
echo $$ > $ROOT/collector.pid

# Background protection, independent of the capture steps (they sleep for minutes): every 2 s the
# game's oom_score_adj is held at 150 or lower (a minimized app gets 900, and lmkd killed the game,
# the largest process, as soon as it was minimized).
( while true; do
    p=$(pidof org.anyps5.android)
    if [ -n "$p" ]; then
        adj=$(cat /proc/$p/oom_score_adj 2>/dev/null)
        [ -n "$adj" ] && [ "$adj" -gt 150 ] 2>/dev/null && echo 150 > /proc/$p/oom_score_adj
    fi
    sleep 2
done ) &

loads() {  # $1 = pid, $2 = output: per-thread CPU % over 3 s
    for t in /proc/$1/task/*; do echo "${t##*/} $(awk '{print $14+$15}' $t/stat 2>/dev/null) $(cat $t/comm 2>/dev/null)"; done > $ROOT/.a1
    sleep 3
    for t in /proc/$1/task/*; do echo "${t##*/} $(awk '{print $14+$15}' $t/stat 2>/dev/null) $(cat $t/comm 2>/dev/null)"; done > $ROOT/.a2
    awk 'NR==FNR{a[$1]=$2;next}{d=$2-a[$1]; if(d>3) printf "%3d%% %s %s\n", d/3, $3, $1}' $ROOT/.a1 $ROOT/.a2 | sort -rn > $2
}

device() {  # battery and thermal zones
    dumpsys battery 2>/dev/null | grep -E 'level|temperature|status'
    for z in /sys/class/thermal/thermal_zone*; do t=$(cat $z/temp 2>/dev/null); [ -n "$t" ] && [ "$t" -gt 30000 ] 2>/dev/null && echo "$(cat $z/type) $t"; done | sort -k2 -rn | head -8
    K=/sys/class/kgsl/kgsl-3d0
    echo "gpu busy% $(cat $K/gpu_busy_percentage 2>/dev/null) clock $(cat $K/gpuclk 2>/dev/null) max $(cat $K/max_gpuclk 2>/dev/null) thermal_pwrlevel $(cat $K/thermal_pwrlevel 2>/dev/null)"
    for c in /sys/devices/system/cpu/cpufreq/policy*; do echo "${c##*/} cur $(cat $c/scaling_cur_freq) max $(cat $c/scaling_max_freq) hw $(cat $c/cpuinfo_max_freq)"; done
}

focused() {  # waits up to 2 min for the game to be on screen; false if it never is
    for i in $(seq 1 60); do
        dumpsys window 2>/dev/null | grep -q "mCurrentFocus.*org.anyps5" && return 0
        [ -n "$(pidof org.anyps5.android)" ] || return 1
        sleep 2
    done
    return 1
}

memory() {  # $1 = pid, $2 = output: where the process memory is (resident, swapped, GPU)
    {
        echo "--- smaps_rollup"; cat /proc/$1/smaps_rollup 2>/dev/null
        echo "--- largest mappings by Rss+Swap (KiB): rss swap name"
        awk '/^[0-9a-f]+-[0-9a-f]+ /{name=$6; if(name=="") name="[anon]"} /^Rss:/{r[name]+=$2} /^Swap:/{w[name]+=$2} END{for(n in r) printf "%d %d %s\n", r[n], w[n], n}' /proc/$1/smaps 2>/dev/null | sort -k1,1nr -k2,2nr | awk '{print; if (++c>=30) exit}'
        echo "--- kgsl (GPU) memory"; for f in /sys/class/kgsl/kgsl/proc/$1/*; do [ -f "$f" ] && echo "${f##*/} $(cat $f 2>/dev/null)"; done
        echo "--- dumpsys meminfo"; dumpsys meminfo $1 2>/dev/null | sed -n '1,30p'
        echo "--- system"; grep -E 'MemTotal|MemAvailable|SwapTotal|SwapFree' /proc/meminfo
    } > $2 2>&1
}

blockmap() {  # $1 = pid, $2 = output folder: box64's translated blocks (read from outside, tools/jitmap.py) + process maps
    LD_LIBRARY_PATH=/data/data/com.termux/files/usr/lib /data/data/com.termux/files/usr/bin/python3 /data/local/tmp/jitmap.py $1 $2/blockmap.txt > $2/blockmap.log 2>&1
    cp /proc/$1/maps $2/maps.txt 2>/dev/null
}

dump() {  # $1 = output folder: one frame dump (the game's own) and a screen capture (only called on screen)
    rm -f $G/frame_000.bmp
    touch $G/dumpnow; sleep 0.3; rm -f $G/dumpnow; sleep 2
    cp $G/frame_000.bmp $1/ 2>/dev/null
    screencap -p $1/screen.png 2>/dev/null
}

onscreen() { dumpsys window 2>/dev/null | grep -q "mCurrentFocus.*org.anyps5"; }

while true; do
    pid=$(pidof org.anyps5.android)
    if [ -z "$pid" ]; then sleep 5; continue; fi
    S=$ROOT/S$(date +%Y%m%d-%H%M%S); mkdir -p $S
    start=$(date +%s)
    { date; echo "pid $pid"; echo "--- env.txt"; cat $G/env.txt; echo "--- libSceAgcDriver.so"; md5sum $(find /data/app -path '*org.anyps5.android*' -name libSceAgcDriver.so | head -1); echo "--- device"; device; } > $S/meta.txt 2>&1
    # A fresh log first: the previous session's markers must not count.
    for i in $(seq 1 60); do grep -qE 'StorageTexture address=.* (512x256 .*vk=37|1280x720)|sdlgpu\] target (512x256|1280x720)' $LOG 2>/dev/null || break; sleep 2; done
    hub=0; combat=0; tick=0; last=$(( $(date +%s) - 900 ))
    while [ -n "$(pidof org.anyps5.android)" ]; do
        if [ $hub = 0 ] && grep -qE 'StorageTexture address=.* 512x256 .*vk=37|sdlgpu\] target 512x256' $LOG 2>/dev/null; then
            hub=1; sleep 15
            if focused; then
                mkdir -p $S/hub; dump $S/hub; loads $pid $S/hub/threads.txt; memory $pid $S/hub/memory.txt
                grep '^\[fps\]' $LOG | tail -10 > $S/hub/fps.txt; device > $S/hub/device.txt 2>&1
                simpleperf record -p $pid -f 500 --call-graph fp --duration 10 -o $S/hub/perf.data > $S/hub/record.log 2>&1
                blockmap $pid $S/hub
            else
                hub=0
            fi
        fi
        if [ $combat = 0 ] && grep -qE 'StorageTexture address=.* 1280x720|sdlgpu\] target 1280x720' $LOG 2>/dev/null; then
            combat=1; sleep 20
            if focused; then
                mkdir -p $S/combat; dump $S/combat; loads $pid $S/combat/threads.txt
                grep '^\[fps\]' $LOG | tail -20 > $S/combat/fps.txt
                device > $S/combat/device.txt 2>&1
                memory $pid $S/combat/memory.txt
                simpleperf record -p $pid -f 500 --call-graph fp --duration 10 -o $S/combat/perf.data > $S/combat/record.log 2>&1
                blockmap $pid $S/combat
            else
                combat=0
            fi
            last=$(date +%s)
        fi
        # 5 min after launch and then every 20 min while the game is on screen: loads, memory, a profile.
        if [ $(( $(date +%s) - last )) -ge 1200 ] && dumpsys window 2>/dev/null | grep -q "mCurrentFocus.*org.anyps5"; then
            last=$(date +%s); P=$S/later-$(date +%H%M); mkdir -p $P
            screencap -p $P/screen.png 2>/dev/null
            loads $pid $P/threads.txt; memory $pid $P/memory.txt; grep '^\[fps\]' $LOG | tail -10 > $P/fps.txt; device > $P/device.txt 2>&1
            simpleperf record -p $pid -f 500 --call-graph fp --duration 10 -o $P/perf.data > $P/record.log 2>&1
            blockmap $pid $P
        fi
        # Keep the game alive in the background: Android gives a minimized app oom_score_adj 900 and
        # lmkd killed the game first (it is the largest process) as soon as it was minimized.
        # 150 keeps it below Termux (keep-termux.sh caps that at 200) and every other cached app.
        adj=$(cat /proc/$pid/oom_score_adj 2>/dev/null)
        [ -n "$adj" ] && [ "$adj" -gt 150 ] 2>/dev/null && echo 150 > /proc/$pid/oom_score_adj
        # A screen capture every 60 s while the game is on screen (shots/HHMMSS.png, at most 40 a session).
        if [ $((tick % 30)) = 15 ] && [ "$(ls $S/shots 2>/dev/null | wc -l)" -lt 40 ] && onscreen; then
            mkdir -p $S/shots; screencap -p $S/shots/$(date +%H%M%S).png 2>/dev/null
        fi
        tick=$((tick + 1))
        if [ $((tick % 30)) = 0 ]; then echo "--- $(date +%T)" >> $S/fps.txt; grep '^\[fps\]' $LOG | tail -6 >> $S/fps.txt; fi
        sleep 2
    done
    { echo "duration $(( $(date +%s) - start )) s"; echo "--- [gpu-map] (last 20)"; grep '^\[gpu-map\]' $LOG | tail -20; echo "--- warnings and errors (count, first 160 chars)"; grep -iE 'error|fail|warn|exception|abort' $LOG 2>/dev/null | cut -c1-160 | sort | uniq -c | sort -rn | head -40; echo "--- crash buffer"; logcat -d -b crash 2>/dev/null | tail -60; } > $S/end.txt
    tail -c 3145728 $LOG > $S/log.txt 2>/dev/null
    # box64's JIT symbol map (BOX64_DYNAREC_PERFMAP=1, TMPDIR=$G): names the "unknown" x86 code in perf.data
    [ -f $G/perf-$pid.map ] && mv $G/perf-$pid.map $S/perf.map
    rm -f $G/perf-*.map
    chmod -R a+rX $S
    ls -d $ROOT/S* 2>/dev/null | sort | head -n -6 | while read old; do rm -rf "$old"; done
done
