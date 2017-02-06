#include <stdio.h>
#include <unistd.h>
#include <wiringPi.h>
#include <inttypes.h>

// register names
#define TFTLCD_DRIV_ID_READ         0x00
#define TFTLCD_DRIV_OUT_CTRL        0x01
#define TFTLCD_DRIV_WAV_CTRL        0x02
#define TFTLCD_ENTRY_MOD            0x03
#define TFTLCD_RESIZE_CTRL          0x04
#define TFTLCD_DISP_CTRL1           0x07
#define TFTLCD_DISP_CTRL2           0x08
#define TFTLCD_DISP_CTRL3           0x09
#define TFTLCD_DISP_CTRL4           0x0A
#define TFTLCD_FRM_MARKER_POS       0x0D
#define TFTLCD_POW_CTRL1            0x10
#define TFTLCD_POW_CTRL2            0x11
#define TFTLCD_POW_CTRL3            0x12
#define TFTLCD_POW_CTRL4            0x13
#define TFTLCD_GRAM_HOR_AD          0x20
#define TFTLCD_GRAM_VER_AD          0x21
#define TFTLCD_RW_GRAM              0x22
#define TFTLCD_VCOMH_CTRL           0x29
#define TFTLCD_FRM_RATE_COL_CTRL    0x2B
#define TFTLCD_GAMMA_CTRL1          0x30
#define TFTLCD_GAMMA_CTRL2          0x31
#define TFTLCD_GAMMA_CTRL3          0x32
#define TFTLCD_GAMMA_CTRL4          0x35
#define TFTLCD_GAMMA_CTRL5          0x36
#define TFTLCD_GAMMA_CTRL6          0x37
#define TFTLCD_GAMMA_CTRL7          0x38
#define TFTLCD_GAMMA_CTRL8		    0x39
#define TFTLCD_GAMMA_CTRL9		    0x3C
#define TFTLCD_GAMMA_CTRL10		    0x3D
#define TFTLCD_HOR_START_AD		    0x50
#define TFTLCD_HOR_END_AD           0x51
#define TFTLCD_VER_START_AD         0x52
#define TFTLCD_VER_END_AD           0x53
#define TFTLCD_GATE_SCAN_CTRL1      0x60
#define TFTLCD_GATE_SCAN_CTRL2      0x61
#define TFTLCD_PART_IMG1_DISP_POS   0x80
#define TFTLCD_PART_IMG1_START_AD   0x81
#define TFTLCD_PART_IMG1_END_AD     0x82
#define TFTLCD_PART_IMG2_DISP_POS   0x83
#define TFTLCD_PART_IMG2_START_AD   0x84
#define TFTLCD_PART_IMG2_END_AD     0x85
#define TFTLCD_PANEL_IF_CTRL1       0x90
#define TFTLCD_PANEL_IF_CTRL2       0x92

struct st7781_pins {
    int data[8];
    int rst;
    int cs;
    int rs;
    int wr;
    int rd;
} default_pins {
    .data = { },
    .rst  =
    .cs  =
    .rs  =
    .wr  =
    .rd  =
};

struct st7781_lcd {
    struct st7781_pins *pins;
    int cursor_x;
    int cursor_y;
};

void st7781_set_data_mode(struct st7781_pins *pins, int mode)
{
    int i;
    for (i = 0; i < sizeof(pins->data); i++) {
        pinMode(pins->data[i], mode);
    }
}

static const uint16_t init_regs[] = {
    TFTLCD_DRIV_OUT_CTRL, 0x0100,
    TFTLCD_DRIV_WAV_CTRL, 0x0700,
    TFTLCD_ENTRY_MOD,  0x1030,
    TFTLCD_DISP_CTRL2, 0x0302,
    TFTLCD_DISP_CTRL3, 0x0000,
    TFTLCD_DISP_CTRL4, 0x0008,

    //*******POWER CONTROL REGISTER INITIAL*******//
    TFTLCD_POW_CTRL1, 0x0790,
    TFTLCD_POW_CTRL2, 0x0005,
    TFTLCD_POW_CTRL3, 0x0000,
    TFTLCD_POW_CTRL4, 0x0000,

     //delayms(50,
    //********POWER SUPPPLY STARTUP 1 SETTING*******//
    TFTLCD_POW_CTRL1, 0x12B0,
    // delayms(50,
     TFTLCD_POW_CTRL2, 0x0007,
     //delayms(50,
    //********POWER SUPPLY STARTUP 2 SETTING******//
    TFTLCD_POW_CTRL3, 0x008C,
    TFTLCD_POW_CTRL4, 0x1700,
    TFTLCD_VCOMH_CTRL, 0x0022,
    // delayms(50,
    //******GAMMA CLUSTER SETTING******//
    TFTLCD_GAMMA_CTRL1, 0x0000,
    TFTLCD_GAMMA_CTRL2, 0x0505,
    TFTLCD_GAMMA_CTRL3, 0x0205,
    TFTLCD_GAMMA_CTRL4, 0x0206,
    TFTLCD_GAMMA_CTRL5, 0x0408,
    TFTLCD_GAMMA_CTRL6, 0x0000,
    TFTLCD_GAMMA_CTRL7, 0x0504,
    TFTLCD_GAMMA_CTRL8, 0x0206,
    TFTLCD_GAMMA_CTRL9, 0x0206,
    TFTLCD_GAMMA_CTRL10, 0x0408,
    // -----------DISPLAY WINDOWS 240*320-------------//
    TFTLCD_HOR_START_AD, 0x0000,
    TFTLCD_HOR_END_AD, 0x00EF,
    TFTLCD_VER_START_AD, 0x0000,
    TFTLCD_VER_END_AD, 0x013F,
    //-----FRAME RATE SETTING-------//
    TFTLCD_GATE_SCAN_CTRL1, 0xA700,
    TFTLCD_GATE_SCAN_CTRL2, 0x0001,
    TFTLCD_PANEL_IF_CTRL1, 0x0033, //RTNI setting
    //-------DISPLAY ON------//
    TFTLCD_DISP_CTRL1, 0x0133,
};

void st7781_init_registers(struct st7781_lcd *lcd, uint16_t *regs, sizeof relems)
{
    int i;
    for (i = 0; i < relems/2; i++) {
        st7781_write_register(regs[i*2], regs[i*2+1]);
    }
}

void st7781_init(struct st7781_lcd *lcd, struct st7781_pins *pins)
{
    wiringPiSetup();
    st7781_set_data_mode(pins, OUTPUT);
    /* Disable the LCD */
    digitalWrite(pins->rst, HIGH);
    pinMode(pins->rst, OUTPUT);

    digitalWrite(pins->cs, HIGH);
    pinMode(pins->cs, OUTPUT);

    digitalWrite(pins->rs, HIGH);
    pinMode(pins->rs, OUTPUT);

    digitalWrite(pins->wr, HIGH);
    pinMode(pins->wr, OUTPUT);

    digitalWrite(pins->rd, HIGH);
    pinMode(pins->rd, OUTPUT);

    lcd->cursor_x = 0;
    lcd->cursor_y = 0;
    lcd->pins     = pins;

    init_registers(lcd, init_regs, sizeof(init_regs));
}

void st7781_reset(struct st7781_lcd *lcd)
{
    struct st7781_pins *pins = lcd->pins;
    digitalWrite(pins->rst, HIGH);
    usleep(2000); // 2 ms
    digitalWrite(pins->rst, LOW);

    // resync
    st7781_write_data(lcd, 0);
    st7781_write_data(lcd, 0);
    st7781_write_data(lcd, 0);
    st7781_write_data(lcd, 0);
}

#define CS_HIGH(pins) (digitalWrite((pins)->cs, HIGH))
#define CS_LOW(pins) (digitalWrite((pins)->cs, LOW))

#define RD_HIGH(pins) (digitalWrite((pins)->rd, HIGH))
#define RD_LOW(pins) (digitalWrite((pins)->rd, HIGH))

#define WR_HIGH(pins) (digitalWrite((pins)->wr, HIGH))
#define WR_LOW(pins) (digitalWrite((pins)->wr, LOW))

#define RS_DATA(pins) (digitalWrite((pins)->rs, LOW))
#define RS_COMMAND(pins) (digitalWrite((pins)->rs, HIGH))

#define st7781_set_write_dir(pins) (st7781_set_data_mode(pins, OUTPUT))
#define st7781_set_read_dir(pins) (st7781_set_data_mode(pins, INPUT))

void st7781_write_byte(struct st7781_pins *pins, uint8_t data)
{
    int i;
    for (i = 0; i < 8; i++) {
        digitalWrite(pins->data[i], (data & (1 << i)) ? HIGH : LOW);
    }
}

uint8_t st7781_write_byte(struct st7781_pins *pins)
{
    int i;
    uint8_t data = 0;
    for (i = 0; i < 8; i++) {
        data |= (digitalRead(pins->data[i])) << i;
    }

    return data;
}

void st7781_write_data(struct st7781_lcd *lcd, uint16_t data)
{
    struct st7781_pins *pins = lcd->pins;
    CS_LOW(pins);

    RS_DATA(pins);
    RD_HIGH(pins);
    WR_HIGH(pins);

    st7781_set_write_dir(pins);
    st7781_write_byte(lcd, data >> 8);

    WR_LOW(pins);
    WR_HIGH(pins);

    st7781_write_byte(lcd, data);
    WR_LOW(pins);
    WR_HIGH(pins);

    CS_HIGH(pins);
}

void st7781_write_command(struct st7781_lcd *lcd, uint16_t cmd)
{
    struct st7781_pins *pins = lcd->pins;
    CS_LOW(pins);

    RS_COMMAND(pins);
    RD_HIGH(pins);
    WR_HIGH(pins);
    st7781_set_write_dir(pins);
    st7781_write_byte(lcd, cmd >> 8);

    WR_LOW(pins);
    WR_HIGH(pins);

    st7781_write_byte(lcd, cmd);

    WR_LOW(pins);
    WR_HIGH(pins);
    CS_HIGH(pins);
}

uint16_t st7781_read_data(struct st7781_lcd *lcd)
{
    uint16_t d = 0;
    struct st7781_pins *pins = lcd->pins;

    CS_LOW(pins);
    RS_DATA(pins);
    RD_HIGH(pins);
    WR_HIGH(pins);
    st7781_read_dir(lcd);

    RD_LOW(pins);
    usleep(10);
    /* Read the higher byte first */
    d = st7781_read_byte(lcd);
    d <<= 8;

    RD_HIGH(pins);
    RD_LOW(pins);

    usleep(10);
    /* Lower byte */
    d |= st7781_read_byte(lcd);
    RD_HIGH(pins);
    CS_HIGH(pins);
    return d;
}

uint16_t st7781_read_register(struct st7781_lcd *lcd, uint16_t addr)
{
    st7781_write_command(lcd, addr);
    return st7781_read_data(lcd);
}

void st7781_write_register(struct st7781_lcd *lcd, uint16_t addr, uint16_t data)
{
    st7781_write_command(lcd, addr);
    st7781_write_data(lcd, addr);
}

int main(int argc, char *argv[])
{
    struct st7781_lcd lcd;
    st7781_init(&lcd, &default_pins);
}
