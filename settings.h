int delete_old_files = 1; // set to 1 and it will delete your oldest day of files so you SD is always 10% empty

// 1 for blink red led with every sd card write, at your frame rate
// 0 for blink only for skipping frames and SOS if camera or sd is broken
#define BlinkWithWrite 1

// reboot startup parameters here

int record_on_reboot = 0; // set to 1 to record, or 0 to NOT record on reboot

// here are 2 sets of startup parameters -- more down in the stop and restart webpage

// VGA 10 fps for 30 minutes, and repeat, play at real time

int framesize = 8;                        //  13 UXGA, 11 HD, 9 SVGA, 8 VGA, 6 CIF
int repeat_config = 100;                  //  repaeat same movie this many times
int xspeed = 1;                           //  playback speed - realtime is 1, or 300 means playpack 30 fps of frames at 10 second per frame ( 30 fps / 0.1 fps )
int gray = 0;                             //  not gray
int quality = 12;                         //  quality on the 10..50 subscale - 10 is good, 20 is grainy and smaller files, 12 is better in bright sunshine due to clipping
int capture_interval = 100;               //  milli-seconds between frames
volatile int total_frames_config = 18000; //  how many frames - length of movie in ms is total_frames x capture_interval

// UXGA 1 frame every 10 seconds for 60 minutes, and repeat, play at 30 fps or 300 times speed
/*
int  framesize = 13;                //  13 UXGA, 11 HD, 9 SVGA, 8 VGA, 6 CIF
int  repeat_config = 300;                 //  repaeat same movie this many times
int  xspeed = 300;                   //  playback speed - realtime is 1, or 300 means playpack 30 fps of frames at 10 second per frames ( 30 fps / 0.1 fps )
int  gray = 0;                     //  not gray
int  quality = 6;                 //  quality on the 10..50 subscale - 10 is good, 20 is grainy and smaller files, 12 is better in bright sunshine due to clipping
int  capture_interval = 10000;       //  milli-seconds between frames
volatile int  total_frames_config = 360;  //  how many frames - length of movie is total_frames x capture_interval
*/
int count_avi = 0;
int count_cam = 0;
int new_config = 5; 
int xlength = total_frames_config * capture_interval / 1000;
int total_frames = total_frames_config;
int recording = 0;
int ready = 0;
int first = 0;
int count = 1;

int diskspeed = 0;
char fname[130];

long current_millis;
long last_capture_millis = 0;
static esp_err_t cam_err;
static esp_err_t card_err;
char strftime_buf[64];
char strftime_buf2[12];

int newfile = 0;
int frames_so_far = 0;
long bp;
long bw;
long totalp;
long totalw;

unsigned long nothing_avi = 0;

