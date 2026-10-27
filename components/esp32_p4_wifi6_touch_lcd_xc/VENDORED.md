# Vendored Waveshare BSP

Copy of `waveshare/esp32_p4_wifi6_touch_lcd_xc` 2.0.0 (upstream commit
395d9adf0ac032311bee6779a699d73739fb9b67), used through `override_path` in
`main/idf_component.yml`.

Local change: `dpi_clock_freq_mhz` 80 -> 48 in `esp32_p4_wifi6_touch_lcd_xc.c`.
The upstream ~131 Hz panel refresh caused rare white flashes with the TRIPLE_FULL
display mode; 48 MHz (~79 Hz) fixes them and makes rendering ~15% faster.
When updating the BSP, re-copy upstream and re-apply this change.
