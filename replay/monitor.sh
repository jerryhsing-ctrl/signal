bpftrace -e '
/* 只抓取 CPU 3 到 6 的事件 */
tracepoint:sched:sched_switch /cpu >= 3 && cpu <= 5/ {
    /* 新增 args->prev_comm 以顯示程式名稱 */
    @[cpu, args->prev_comm, args->prev_state] = count();
}

interval:s:5 {
    time("%H:%M:%S ");
    print(@);
    clear(@);
}
'