FILE *avifile = NULL;
FILE *idxfile = NULL;

#define AVIOFFSET 240 // AVI main header length

unsigned long movi_size = 0;
unsigned long jpeg_size = 0;
unsigned long idx_offset = 0;

uint8_t zero_buf[4] = {0x00, 0x00, 0x00, 0x00};
uint8_t dc_buf[4] = {0x30, 0x30, 0x64, 0x63};   // "00dc"
uint8_t avi1_buf[4] = {0x41, 0x56, 0x49, 0x31}; // "AVI1"
uint8_t idx1_buf[4] = {0x69, 0x64, 0x78, 0x31}; // "idx1"

uint8_t vga_w[2] = {0x80, 0x02};  // 640
uint8_t vga_h[2] = {0xE0, 0x01};  // 480
uint8_t cif_w[2] = {0x90, 0x01};  // 400
uint8_t cif_h[2] = {0x28, 0x01};  // 296
uint8_t svga_w[2] = {0x20, 0x03}; // 800
uint8_t svga_h[2] = {0x58, 0x02}; // 600
uint8_t hd_w[2] = {0x00, 0x05};   // 1280
uint8_t hd_h[2] = {0xD0, 0x02};   // 720
uint8_t uxga_w[2] = {0x40, 0x06}; // 1600
uint8_t uxga_h[2] = {0xB0, 0x04}; // 1200

const int avi_header[AVIOFFSET] PROGMEM = {
    0x52,
    0x49,
    0x46,
    0x46,
    0xD8,
    0x01,
    0x0E,
    0x00,
    0x41,
    0x56,
    0x49,
    0x20,
    0x4C,
    0x49,
    0x53,
    0x54,
    0xD0,
    0x00,
    0x00,
    0x00,
    0x68,
    0x64,
    0x72,
    0x6C,
    0x61,
    0x76,
    0x69,
    0x68,
    0x38,
    0x00,
    0x00,
    0x00,
    0xA0,
    0x86,
    0x01,
    0x00,
    0x80,
    0x66,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x10,
    0x00,
    0x00,
    0x00,
    0x64,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x80,
    0x02,
    0x00,
    0x00,
    0xe0,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x4C,
    0x49,
    0x53,
    0x54,
    0x84,
    0x00,
    0x00,
    0x00,
    0x73,
    0x74,
    0x72,
    0x6C,
    0x73,
    0x74,
    0x72,
    0x68,
    0x30,
    0x00,
    0x00,
    0x00,
    0x76,
    0x69,
    0x64,
    0x73,
    0x4D,
    0x4A,
    0x50,
    0x47,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x0A,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x73,
    0x74,
    0x72,
    0x66,
    0x28,
    0x00,
    0x00,
    0x00,
    0x28,
    0x00,
    0x00,
    0x00,
    0x80,
    0x02,
    0x00,
    0x00,
    0xe0,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x18,
    0x00,
    0x4D,
    0x4A,
    0x50,
    0x47,
    0x00,
    0x84,
    0x03,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x49,
    0x4E,
    0x46,
    0x4F,
    0x10,
    0x00,
    0x00,
    0x00,
    0x6A,
    0x61,
    0x6D,
    0x65,
    0x73,
    0x7A,
    0x61,
    0x68,
    0x61,
    0x72,
    0x79,
    0x20,
    0x76,
    0x41,
    0x31,
    0x20,
    0x4C,
    0x49,
    0x53,
    0x54,
    0x00,
    0x01,
    0x0E,
    0x00,
    0x6D,
    0x6F,
    0x76,
    0x69,
};
