
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// GLOBALS
#define BUFFSIZE 512

// global variable used by these pieces

uint8_t buf[BUFFSIZE];

static int i = 0;
unsigned long fileposition = 0;
uint16_t frame_cnt = 0;
uint16_t remnant = 0;
uint32_t startms;
uint32_t elapsedms;
uint32_t uVideoLen = 0;
int other_cpu_active = 0;
int skipping = 0;
int skipped = 0;
int bad_jpg = 0;
int extend_jpg = 0;
int normal_jpg = 0;

int fb_max = 12;

camera_fb_t *fb_q[30];
int fb_in = 0;
int fb_out = 0;

camera_fb_t *fb = NULL;