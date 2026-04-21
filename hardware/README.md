# hardware/

KiCad schematic and PCB for the gio module.

**This directory is intentionally empty until breadboard validation is complete.**

Per `docs/decisions.md` §7: the KiCad project is deferred until the breadboard signal chain (MCP4725 → MCP6002 → V/Oct output) is validated within ±2 mV of target voltages and the I2C bus sharing between DAC and OLED is confirmed clean.

The gate resistor value (R4) was a known source of V/Oct loading error in the predecessor project — confirmed to 100 Ω before PCB layout begins.

When ready, this directory will contain:

```
hardware/
├── gio.kicad_pro
├── gio.kicad_sch
├── gio.kicad_pcb
├── panel/
│   └── gio-panel.kicad_pcb
├── gerbers/
│   └── (generated outputs for JLCPCB)
└── bom/
    └── gio-bom.csv
```
