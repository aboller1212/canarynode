set pagination off
set confirm off
target extended-remote | openocd -c "gdb_port pipe; log_output /tmp/oocd.log" -f interface/stlink.cfg -f target/stm32g4x.cfg
monitor reset halt
load
monitor reset halt
break main.c:52
commands
silent
printf "T=%.2f C   P=%.2f hPa   H=%.2f %%RH\n", temp_c, press_hpa, hum_rh
continue
end
continue
