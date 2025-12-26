# Northern Chorder Hardware Design

Custom chording keyboard using capacitive touch sensors instead of mechanical buttons.

## Photo Reference

| Components | XIAO Front | XIAO Back | Trill Square | Trill Bar |
|:----------:|:----------:|:---------:|:------------:|:---------:|
| <a href="../../photos/nChorder/nChorder_components.jpg"><img src="../../photos/nChorder/nChorder_components.jpg" width="120"></a> | <a href="../../photos/nChorder/01_xiao_nrf52840_plus_front_pinout.jpg"><img src="../../photos/nChorder/01_xiao_nrf52840_plus_front_pinout.jpg" width="120"></a> | <a href="../../photos/nChorder/02_xiao_nrf52840_plus_back.jpg"><img src="../../photos/nChorder/02_xiao_nrf52840_plus_back.jpg" width="120"></a> | <a href="../../photos/nChorder/03_trill_square_back_pinout.jpg"><img src="../../photos/nChorder/03_trill_square_back_pinout.jpg" width="120"></a> | <a href="../../photos/nChorder/04_trill_bar_back_pinout.jpg"><img src="../../photos/nChorder/04_trill_bar_back_pinout.jpg" width="120"></a> |

## Bill of Materials

| Qty | Component | Description | Notes |
|-----|-----------|-------------|-------|
| 1 | Seeed Studio XIAO nRF52840-Plus | MCU module with BLE, USB-C, battery charging | Model: XIAO-nRF52840-Plus |
| 1 | 2000mAh LiPo battery | 3.7V lithium polymer | JST connector |
| 3 | Bela Trill Bar | I2C capacitive 1D multi-touch sensor | 26 electrodes each |
| 1 | Bela Trill Square | I2C capacitive 2D touch sensor | X/Y position + pressure |
| 1 | Adafruit PCA9548 QT | 8-channel I2C multiplexer | STEMMA QT connectors |

## Component Details

### Seeed XIAO nRF52840-Plus

<a href="../../photos/nChorder/01_xiao_nrf52840_plus_front_pinout.jpg"><img src="../../photos/nChorder/01_xiao_nrf52840_plus_front_pinout.jpg" width="200"></a>

**Pinout** (from silkscreen on PCB):

```
                    USB-C
              ┌───────────────┐
              │  CLK DIO      │  (debug pads)
              │  GND RST      │
              │               │
        VUSB  │●             ●│ D0
         GND  │●             ●│ D1
         3V3  │●             ●│ D2
         D10  │●             ●│ D3
          D9  │●             ●│ D4 (SDA)
          D8  │●             ●│ D5 (SCL)
          D7  │●             ●│ D6
              │               │
              │    BAT + -    │  (battery pads)
              └───────────────┘
                   Antenna
```

**Key Pins:**
| Pin | Function | Notes |
|-----|----------|-------|
| D4 | I2C SDA | Default I2C data |
| D5 | I2C SCL | Default I2C clock |
| D0-D3 | Analog/Digital GPIO | A0-A3 capable |
| D6-D10 | Digital GPIO | D7=SCK, D8=MISO, D9=MOSI |
| 3V3 | 3.3V output | Regulated from USB or battery |
| VUSB | 5V USB power | Only when USB connected |
| BAT+/- | LiPo pads | Built-in BQ25100 charger |
| CLK/DIO | SWD debug | For programming/debug |

**Power:**
- Operating voltage: 3.3V
- Battery charging: Built-in BQ25100 charger via USB-C
- Battery connector: Solder pads (not JST)

### Bela Trill Sensors

All Trill sensors use I2C with **default address 0x20**. Since we have 4 sensors, we need the I2C multiplexer.

**Trill Bar:**
- 26 capacitive electrodes in a row
- Reports up to 5 simultaneous touches
- Each touch: position (0-3200) and size (pressure proxy)
- Dimensions: 60mm x 8mm active area

**Trill Square:**
- 2D capacitive touch surface
- Reports X, Y position and touch size
- Up to 5 simultaneous touches
- Dimensions: 42mm x 38mm active area

#### Trill Bar Pinout

<a href="../../photos/nChorder/04_trill_bar_back_pinout.jpg"><img src="../../photos/nChorder/04_trill_bar_back_pinout.jpg" width="200"></a>

6-pin header (active area at bottom, connector at top):

```
        ┌─────┐
    RST │●    │
    EVT │●    │  ┌──────┐
    GND │●    │  │ QWIIC│
    VCC │●    │  └──────┘
    SDA │●    │
    SCL │●    │
        └─────┘
       [ADR 0 1]

    ════════════════
      [Touch Area]
    ════════════════
```

#### Trill Square Pinout

<a href="../../photos/nChorder/03_trill_square_back_pinout.jpg"><img src="../../photos/nChorder/03_trill_square_back_pinout.jpg" width="200"></a>

J1 6-pin header (active area at bottom):

```
    ┌─────────────────────────┐
    │ SCL SDA VCC GND EVT RST │  J1
    │  ●   ●   ●   ●   ●   ●  │
    │              ┌──────┐   │
    │   [ADR 0 1]  │ QWIIC│   │
    │              └──────┘   │
    │                         │
    │    ═══════════════      │
    │      [Touch Area]       │
    │    ═══════════════      │
    └─────────────────────────┘
```

#### Trill Pin Functions

| Pin | Function | Required |
|-----|----------|----------|
| SCL | I2C Clock | Yes |
| SDA | I2C Data | Yes |
| VCC | 3.3V power | Yes |
| GND | Ground | Yes |
| EVT | Event/interrupt output | Optional |
| RST | Reset input (active low) | Optional |

**Address Selection (ADR jumpers):**
- Default address: 0x20
- ADR0 shorted: 0x21
- ADR1 shorted: 0x22
- Both shorted: 0x23

**Note:** With the I2C multiplexer, we leave all sensors at default 0x20.

### Adafruit PCA9548 I2C Multiplexer

8-channel I2C multiplexer allowing multiple devices with same address.

**Board Pinout:**

```
      STEMMA QT (upstream)
           ┌───┐
    ┌──────┤   ├──────┐
    │  VIN ●   ● GND  │
    │  SDA ●   ● SCL  │
    │  RST ●   ● A0   │
    │   A1 ●   ● A2   │
    │                 │
    │ SD0 ● ● SC0     │  Channel 0
    │ SD1 ● ● SC1     │  Channel 1
    │ SD2 ● ● SC2     │  Channel 2
    │ SD3 ● ● SC3     │  Channel 3
    │ SD4 ● ● SC4     │  Channel 4
    │ SD5 ● ● SC5     │  Channel 5
    │ SD6 ● ● SC6     │  Channel 6
    │ SD7 ● ● SC7     │  Channel 7
    │                 │
    └─────────────────┘
      STEMMA QT (downstream)
```

**Multiplexer Address:** 0x70 (default, configurable via A0-A2)

| A2 | A1 | A0 | Address |
|----|----|----|---------|
| 0 | 0 | 0 | 0x70 |
| 0 | 0 | 1 | 0x71 |
| ... | ... | ... | ... |
| 1 | 1 | 1 | 0x77 |

## Wiring Plan

### Power Distribution

```
                    USB-C (5V)
                       │
                       ▼
              ┌────────────────┐
              │ XIAO nRF52840  │
              │                │
    LiPo ────►│ BAT+      3V3  │────┬────► PCA9548 VIN
    2000mAh   │ BAT-      GND  │────┼────► PCA9548 GND
              └────────────────┘    │
                                    ├────► Trill Bar 1 (VCC, GND)
                                    ├────► Trill Bar 2 (VCC, GND)
                                    ├────► Trill Bar 3 (VCC, GND)
                                    └────► Trill Square (VCC, GND)
```

### I2C Bus Topology

```
XIAO nRF52840                      PCA9548 (0x70)                 Trill Sensors
┌───────────┐                    ┌─────────────────┐
│       SDA ├───────────────────►│ SDA        SD0 ├────► Trill Square (0x20)
│       SCL ├───────────────────►│ SCL        SC0 │
│        D6 ├───────────────────►│ RST        SD1 ├────► Trill Bar 1 (0x20)
│           │                    │            SC1 │
│           │                    │            SD2 ├────► Trill Bar 2 (0x20)
│           │                    │            SC2 │
│           │                    │            SD3 ├────► Trill Bar 3 (0x20)
│           │                    │            SC3 │
└───────────┘                    └─────────────────┘
```

### Wire Color Convention

Following standard practice visible in reference photo:

| Color | Signal |
|-------|--------|
| Red | 3.3V / VCC |
| Black | GND |
| Yellow | SDA (I2C Data) |
| Blue/White | SCL (I2C Clock) |

### Complete Connection Table

| From | Pin | To | Pin | Wire Color | Signal |
|------|-----|----|-----|------------|--------|
| **Power** |
| XIAO | 3V3 | PCA9548 | VIN | Red | 3.3V supply |
| XIAO | GND | PCA9548 | GND | Black | Ground |
| XIAO | 3V3 | Trill Bar 1 | VCC | Red | 3.3V supply |
| XIAO | GND | Trill Bar 1 | GND | Black | Ground |
| XIAO | 3V3 | Trill Bar 2 | VCC | Red | 3.3V supply |
| XIAO | GND | Trill Bar 2 | GND | Black | Ground |
| XIAO | 3V3 | Trill Bar 3 | VCC | Red | 3.3V supply |
| XIAO | GND | Trill Bar 3 | GND | Black | Ground |
| XIAO | 3V3 | Trill Square | VCC | Red | 3.3V supply |
| XIAO | GND | Trill Square | GND | Black | Ground |
| LiPo | + | XIAO | BAT+ | Red | Battery positive |
| LiPo | - | XIAO | BAT- | Black | Battery negative |
| **I2C Main Bus** |
| XIAO | D4 (SDA) | PCA9548 | SDA | Yellow | I2C Data |
| XIAO | D5 (SCL) | PCA9548 | SCL | Blue | I2C Clock |
| **Control** |
| XIAO | D6 | PCA9548 | RST | - | Reset (active low) |
| **I2C Mux Channels** |
| PCA9548 | SD0 | Trill Square | SDA | Yellow | Ch0 Data (thumb) |
| PCA9548 | SC0 | Trill Square | SCL | Blue | Ch0 Clock |
| PCA9548 | SD1 | Trill Bar 1 | SDA | Yellow | Ch1 Data (finger row 1) |
| PCA9548 | SC1 | Trill Bar 1 | SCL | Blue | Ch1 Clock |
| PCA9548 | SD2 | Trill Bar 2 | SDA | Yellow | Ch2 Data (finger row 2) |
| PCA9548 | SC2 | Trill Bar 2 | SCL | Blue | Ch2 Clock |
| PCA9548 | SD3 | Trill Bar 3 | SDA | Yellow | Ch3 Data (finger row 3) |
| PCA9548 | SC3 | Trill Bar 3 | SCL | Blue | Ch3 Clock |

## I2C Software Configuration

### Multiplexer Channel Selection

To communicate with a specific Trill sensor, first select the PCA9548 channel:

```c
#define PCA9548_ADDR  0x70
#define TRILL_ADDR    0x20
#define MUX_RST_PIN   D6  // GPIO for mux reset

// Reset the multiplexer
void reset_mux(void) {
    nrf_gpio_pin_clear(MUX_RST_PIN);
    nrf_delay_us(10);
    nrf_gpio_pin_set(MUX_RST_PIN);
}

// Select multiplexer channel (0-7)
void select_mux_channel(uint8_t channel) {
    uint8_t data = (1 << channel);  // Enable one channel
    i2c_write(PCA9548_ADDR, &data, 1);
}

// Example: Read from Trill Square (thumb)
select_mux_channel(0);  // Channel 0
trill_read(TRILL_ADDR);

// Example: Read from Trill Bar 2 (finger row 2)
select_mux_channel(2);  // Channel 2
trill_read(TRILL_ADDR);
```

### Sensor Assignment

| Mux Channel | Sensor | Intended Use |
|-------------|--------|--------------|
| 0 | Trill Square | Thumb control (tap/mouse/scroll) |
| 1 | Trill Bar 1 | Finger row 1 (index) |
| 2 | Trill Bar 2 | Finger row 2 (middle) |
| 3 | Trill Bar 3 | Finger row 3 (ring/pinky) |

## Mechanical Considerations

### Finger Button Mapping

Each Trill Bar has 26 electrodes across 60mm. For 4 finger buttons per row:

```
    Trill Bar electrode mapping (4 buttons)
    ┌────────────────────────────────────────────────┐
    │  ████████  ████████  ████████  ████████        │
    │  Button 1  Button 2  Button 3  Button 4        │
    │  (0-6)     (7-12)    (13-19)   (20-25)         │
    └────────────────────────────────────────────────┘
         15mm      15mm      15mm      15mm
```

**Touch zones** (approximate electrode ranges):
- Button 1: electrodes 0-6 (position 0-800)
- Button 2: electrodes 7-12 (position 800-1600)
- Button 3: electrodes 13-19 (position 1600-2400)
- Button 4: electrodes 20-25 (position 2400-3200)

### Thumb Control

Trill Square can function as:
- **Thumb button**: Tap detection for chord input
- **Mouse/trackpad**: X/Y position for cursor control
- **Scroll**: Swipe gestures for scrolling

## Verification Status

**Verified from photos/datasheets:**
- [x] XIAO pinout (D4=SDA, D5=SCL per silkscreen)
- [x] Trill Bar pinout (6-pin: RST, EVT, GND, VCC, SDA, SCL)
- [x] Trill Square pinout (6-pin: SCL, SDA, VCC, GND, EVT, RST)
- [x] Trill default I2C address 0x20

**Wired up:**
- [x] XIAO to PCA9548 (VIN, GND, SDA, SCL, RST)
- [x] Trill Square on mux channel 0

**Not yet tested:**
- [ ] XIAO I2C communication with PCA9548
- [ ] PCA9548 channel selection protocol
- [ ] Trill Square responding through mux ch0
- [ ] Trill Bars on mux channels 1-3
- [ ] Power consumption within XIAO 3V3 rail capacity
- [ ] I2C pull-up requirements (internal vs external)
- [ ] Battery charging and runtime

## References

- [Seeed XIAO nRF52840 Wiki](https://wiki.seeedstudio.com/XIAO_BLE/)
- [Bela Trill Documentation](https://learn.bela.io/products/trill/)
- [Adafruit PCA9548 Guide](https://learn.adafruit.com/adafruit-pca9548-8-channel-stemma-qt-qwiic-i2c-multiplexer)
- [PCA9548A Datasheet](https://www.ti.com/lit/ds/symlink/pca9548a.pdf)
