/* 480x272 */
static struct s3c2410fb_mach_info smdk2440_lcd_cfg __initdata = {
    .regs   = {
        .lcdcon1 =  S3C2410_LCDCON1_TFT16BPP | \
                S3C2410_LCDCON1_TFT | \
                                  S3C2410_LCDCON1_CLKVAL(0x04),

        .lcdcon2 =  S3C2410_LCDCON2_VBPD(1) | \
                S3C2410_LCDCON2_LINEVAL(271) | \
                S3C2410_LCDCON2_VFPD(1) | \
                S3C2410_LCDCON2_VSPW(9),

        .lcdcon3 =  S3C2410_LCDCON3_HBPD(1) | \
                S3C2410_LCDCON3_HOZVAL(479) | \
                S3C2410_LCDCON3_HFPD(1),

        .lcdcon4 =  S3C2410_LCDCON4_HSPW(40),

                .lcdcon5        = S3C2410_LCDCON5_FRM565 |
                                  S3C2410_LCDCON5_INVVLINE |
                                  S3C2410_LCDCON5_INVVFRAME |
                                  S3C2410_LCDCON5_PWREN |
                                  S3C2410_LCDCON5_HWSWP,
        },
.gpccon      =  0xaaaaaaaa,
        .gpccon_mask    = 0xffffffff,
    .gpcup       =  0xffffffff,
        .gpcup_mask     = 0xffffffff,

    .gpdcon      =  0xaaaaaaaa,
        .gpdcon_mask    = 0xffffffff,
    .gpdup       =  0xffffffff,
        .gpdup_mask     = 0xffffffff,

    .fixed_syncs =  1,
    .type        =  S3C2410_LCDCON1_TFT,
        .width          = 480,
        .height         = 272,

        .xres           = {
                .min    = 480,
                .max    = 480,
                .defval = 480,
        },

        .yres           = {
        .max    =   272,
        .min    =   272,
        .defval =   272,
    },
        .bpp    = {
        .min    =   16,
        .max    =   16,
        .defval =   16,
    },
};

