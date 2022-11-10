#include "ESP32CamLib.h"
#include <dummy.h>
#include <stdio.h>
#include <SD_MMC.h>

#include "settings.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

// MicroSD
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

// CAMERA_MODEL_AI_THINKER
#include "camSettings.h"
// AVI FILE SETTINGS
#include "aviSettings.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/cpu.h"
#include "soc/rtc_cntl_reg.h"

#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "nvs.h"

TaskHandle_t CameraTask, AviWriterTask;
SemaphoreHandle_t baton;

void codeForAviWriterTask(void *parameter)
{
    uint32_t ulNotifiedValue;
    Serial.print("aviwriter, core ");
    Serial.print(xPortGetCoreID());
    Serial.print(", priority = ");
    Serial.println(uxTaskPriorityGet(NULL));

    for (;;)
    {
        ulNotifiedValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (ulNotifiedValue-- > 0)
        {
            make_avi();
            count_avi++;
            delay(1);
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// CameraTask runs on cpu 1 to take pictures and drop them in a queue
//

void codeForCameraTask(void *parameter)
{
    int pic_delay = 0;
    int next = 0;
    long next_run_time = 0;
    Serial.print("camera, core ");
    Serial.print(xPortGetCoreID());
    Serial.print(", priority = ");
    Serial.println(uxTaskPriorityGet(NULL));

    for (;;)
    {

        if (other_cpu_active == 1)
        {
            current_millis = millis();
            count_cam++;
            xSemaphoreTake(baton, portMAX_DELAY);

            int q_size = (fb_in + fb_max - fb_out) % fb_max;

            if (q_size + 1 == fb_max)
            {
                xSemaphoreGive(baton);

                Serial.print(" Queue Full, Skipping ... "); // the queue is full
                skipped++;
                skipped++;
                skipping = 1;
                next = 3 * capture_interval;
            }
            else
            {
                frames_so_far++;
                frame_cnt++;

                fb_in = (fb_in + 1) % fb_max;
                bp = millis();

                do
                {
                    fb_q[fb_in] = esp_camera_fb_get();
                    int x = fb_q[fb_in]->len;
                    int foundffd9 = 0;

                    for (int j = 1; j <= 1025; j++)
                    {
                        if (fb_q[fb_in]->buf[x - j] != 0xD9)
                        {
                        }
                        else
                        {

                            if (fb_q[fb_in]->buf[x - j - 1] == 0xFF)
                            {

                                if (j == 1)
                                {
                                    normal_jpg++;
                                }
                                else
                                {
                                    extend_jpg++;
                                }
                                if (j > 1000)
                                { //  never happens. but > 1 does, usually 400-500
                                    Serial.print("Frame ");
                                    Serial.print(frames_so_far);
                                    Serial.print(", Len = ");
                                    Serial.print(x);
                                    Serial.print(", Corrent Len = ");
                                    Serial.print(x - j + 1);
                                    Serial.print(", Extra Bytes = ");
                                    Serial.println(j - 1);
                                }
                                foundffd9 = 1;
                                break;
                            }
                        }
                    }

                    if (!foundffd9)
                    {
                        bad_jpg++;
                        Serial.print("Bad jpeg, Len = ");
                        Serial.println(x);
                        esp_camera_fb_return(fb_q[fb_in]);
                    }
                    else
                    {
                        break;
                    }

                } while (1);

                totalp = totalp - bp + millis();
                pic_delay = millis() - current_millis;
                xSemaphoreGive(baton);
                last_capture_millis = millis();

                if (q_size == 0)
                {
                    if (skipping == 1)
                    {
                        Serial.println(" Queue cleared. ");
                        skipping = 0;
                    }
                    next = capture_interval - pic_delay;
                    if (next < 2)
                        next = 2;
                }
                else if (q_size < 2)
                {
                    next = capture_interval - pic_delay;
                    if (next < 2)
                        next = 2;
                }
                else if (q_size < 4)
                {
                    next = capture_interval;
                }
                else
                {
                    next = 2 * capture_interval;
                    skipped++;
                }
            }

            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR(AviWriterTask, &xHigherPriorityTaskWoken);

            delay(next);
            next_run_time = millis() + next;
        }
        else
        {
            next_run_time = millis() + capture_interval;
            delay(capture_interval);
        }
    }
}

//
// Writes an uint32_t in Big Endian at current file position
//
static void inline print_quartet(unsigned long i, FILE *fd)
{
    uint8_t y[4];

    y[0] = i % 0x100;
    y[1] = (i >> 8) % 0x100;
    y[2] = (i >> 16) % 0x100;
    y[3] = (i >> 24) % 0x100;

    size_t i1_err = fwrite(y, 1, 4, fd);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  delete_old_stuff() - before every avi, delete oldest day if freespace less than 10%
//
//  code copied from user @gemi254

void delete_old_stuff()
{

    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

    float full = 1.0 * SD_MMC.usedBytes() / SD_MMC.totalBytes();
    ;
    if (full < 0.8)
    {
        Serial.printf("Nothing deleted, %.1f%% disk full\n", 100.0 * full);
    }
    else
    {
        Serial.printf("Disk is %.1f%% full ... deleting oldest day\n", 100.0 * full);
        int oldest = 22222222;
        char oldestname[50];

        File f = SD_MMC.open("/");
        if (f.isDirectory())
        {

            File file = f.openNextFile();
            while (file)
            {
                if (file.isDirectory())
                {

                    char foldname[50];
                    strcpy(foldname, file.name());
                    int i = atoi(foldname);
                    if (i > 20200000 && i < oldest)
                    {
                        strcpy(oldestname, file.name());
                        oldest = i;
                    }
                }
                file = f.openNextFile();
            }
            deleteFolderOrFile(oldestname);
            f.close();
        }
    }
}

void deleteFolderOrFile(const char *val)
{
    char val_w_slash[60];
    sprintf(val_w_slash, "/%s", val);
    Serial.printf("Deleting : %s\n", val_w_slash);
    File f = SD_MMC.open(val_w_slash);
    if (!f)
    {
        Serial.printf("Failed to open %s\n", val_w_slash);
        return;
    }

    if (f.isDirectory())
    {
        File file = f.openNextFile();
        while (file)
        {
            if (file.isDirectory())
            {
                Serial.print("  DIR : ");
                Serial.println(file.name());
            }
            else
            {
                char file_w_slash[100];
                sprintf(file_w_slash, "%s/%s", val_w_slash, file.name());
                Serial.print("  FILE: ");
                Serial.print(file_w_slash);
                Serial.print("  SIZE: ");
                Serial.print(file.size());

                if (SD_MMC.remove(file_w_slash))
                {
                    Serial.println(" deleted.");
                }
                else
                {
                    Serial.println(" FAILED.");
                }
            }
            file = f.openNextFile();
        }
        f.close();
        // Remove the dir
        if (SD_MMC.rmdir(val_w_slash))
        {
            Serial.printf("Dir %s removed\n", val_w_slash);
        }
        else
        {
            Serial.println("Remove dir failed");
        }
    }
    else
    {
        // Remove the file
        if (SD_MMC.remove(val_w_slash))
        {
            Serial.printf("File %s deleted\n", val_w_slash);
        }
        else
        {
            Serial.println("Delete failed");
        }
    }
}

void init_tasks()
{

    Serial.begin(115200);
    Serial.println("\n\n---");

    rtc_gpio_hold_dis(GPIO_NUM_33);
    pinMode(33, OUTPUT);   // little red led on back of chip
    digitalWrite(33, LOW); // turn on the red LED on the back of chip

    rtc_gpio_hold_dis(GPIO_NUM_4);
    pinMode(4, OUTPUT);   // Blinding Disk-Avtive Light
    digitalWrite(4, LOW); // turn off

    Serial.setDebugOutput(true);
    Serial.print("setup, core ");
    Serial.print(xPortGetCoreID());
    Serial.print(", priority = ");
    Serial.println(uxTaskPriorityGet(NULL));

    total_frames = total_frames_config;

    if (!psramFound())
    {
        Serial.println("paraFound wrong - major fail");
        major_fail();
    }

    Serial.println("Starting sd card ...");

    // SD camera init
    card_err = init_sdcard();
    if (card_err != ESP_OK)
    {
        Serial.printf("SD Card init failed with error 0x%x", card_err);
        major_fail();
        return;
    }

    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

    Serial.println("Starting tasks ...");

    baton = xSemaphoreCreateMutex(); // baton controls access to camera and frame queue

    xTaskCreatePinnedToCore(
        codeForCameraTask,
        "CameraTask",
        1024, // heap available for this task
        NULL,
        2, // prio 2 - higher number is higher priio
        &CameraTask,
        1); // core 1

    delay(20);

    xTaskCreatePinnedToCore(
        codeForAviWriterTask,
        "AviWriterTask",
        3072, // heap
        NULL,
        3, // prio 3
        &AviWriterTask,
        0); // on cpu 0

    delay(20);

    Serial.println("Starting camera ...");

    recording = 0; // we are NOT recording
    config_camera();

    newfile = 0; // no file is open  // don't fiddle with this!

    recording = record_on_reboot;

    ready = 1;
    digitalWrite(33, HIGH); // red light turns off when setup is complete

    xTaskNotifyGive(AviWriterTask);

    delay(1000);

    if (delete_old_files)
        delete_old_stuff();
}

//
// if we have no camera, or sd card, then flash rear led on and off to warn the human SOS - SOS
//
void major_fail()
{

    Serial.println(" ");

    for (int i = 0; i < 10; i++)
    { // 10 loops or about 100 seconds then reboot
        for (int j = 0; j < 3; j++)
        {
            digitalWrite(33, LOW);
            delay(150);
            digitalWrite(33, HIGH);
            delay(150);
        }

        delay(1000);

        for (int j = 0; j < 3; j++)
        {
            digitalWrite(33, LOW);
            delay(500);
            digitalWrite(33, HIGH);
            delay(500);
        }

        delay(1000);
        Serial.print("Major Fail  ");
        Serial.print(i);
        Serial.print(" / ");
        Serial.println(10);
    }

    ESP.restart();
}

static esp_err_t init_sdcard()
{

    // pinMode(12, PULLUP);
    pinMode(13, PULLUP);
    // pinMode(4, OUTPUT);

    esp_err_t ret = ESP_FAIL;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT; // using 1 bit mode
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    diskspeed = host.max_freq_khz;
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // using 1 bit mode
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
    };

    sdmmc_card_t *card;

    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret == ESP_OK)
    {
        Serial.println("SD card mount successfully!");
    }
    else
    {
        Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
        Serial.println("Try again...");
        delay(5000);
        diskspeed = 400;
        host.max_freq_khz = SDMMC_FREQ_PROBING;
        ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
        if (ret == ESP_OK)
        {
            Serial.println("SD card mount successfully SLOW SLOW SLOW");
        }
        else
        {
            Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
            major_fail();
        }
    }
    sdmmc_card_print_info(stdout, card);
    Serial.print("SD_MMC Begin: ");
    Serial.println(SD_MMC.begin());
    return ret;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Make the avi move in 4 pieces
//
// make_avi() called in every loop, which calls below, depending on conditions
//   start_avi() - open the file and write headers
//   another_pic_avi() - write one more frame of movie
//   end_avi() - write the final parameters and close the file

void make_avi()
{

    if (newfile == 0 && recording == 1)
    { // open the file

        digitalWrite(33, HIGH);
        newfile = 1;

        if (delete_old_files)
            delete_old_stuff();

        start_avi(); // now start the avi
    }
    else
    {

        // we have a file open, but not recording

        if (newfile == 1 && recording == 0)
        { // got command to close file

            digitalWrite(33, LOW);
            end_avi();

            Serial.println("Done capture due to command");

            frames_so_far = total_frames;

            newfile = 0;   // file is closed
            recording = 0; // DO NOT start another recording
        }
        else
        {

            if (newfile == 1 && recording == 1)
            { // regular recording

                if ((millis() - startms) > (total_frames * capture_interval))
                { // time is up, even though we have not done all the frames

                    Serial.println(" ");
                    Serial.println("Done capture for time");
                    Serial.print("Time Elapsed: ");
                    Serial.print(millis() - startms);
                    Serial.print(" Frames: ");
                    Serial.println(frame_cnt);
                    Serial.print("Config:       ");
                    Serial.print(total_frames * capture_interval);
                    Serial.print(" (");
                    Serial.print(total_frames);
                    Serial.print(" x ");
                    Serial.print(capture_interval);
                    Serial.println(")");

                    digitalWrite(33, LOW); // close the file

                    end_avi();

                    frames_so_far = 0;
                    newfile = 0; // file is closed
                    recording = 0;
                }
                else
                { // regular

                    another_save_avi();
                }
            }
        }
    }
}

static void config_camera()
{

    camera_config_t config;

    if (new_config == 5)
    {

        config.ledc_channel = LEDC_CHANNEL_0;
        config.ledc_timer = LEDC_TIMER_0;
        config.pin_d0 = Y2_GPIO_NUM;
        config.pin_d1 = Y3_GPIO_NUM;
        config.pin_d2 = Y4_GPIO_NUM;
        config.pin_d3 = Y5_GPIO_NUM;
        config.pin_d4 = Y6_GPIO_NUM;
        config.pin_d5 = Y7_GPIO_NUM;
        config.pin_d6 = Y8_GPIO_NUM;
        config.pin_d7 = Y9_GPIO_NUM;
        config.pin_xclk = XCLK_GPIO_NUM;
        config.pin_pclk = PCLK_GPIO_NUM;
        config.pin_vsync = VSYNC_GPIO_NUM;
        config.pin_href = HREF_GPIO_NUM;
        config.pin_sscb_sda = SIOD_GPIO_NUM;
        config.pin_sscb_scl = SIOC_GPIO_NUM;
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = 20000000;
        config.pixel_format = PIXFORMAT_JPEG;

        config.frame_size = FRAMESIZE_UXGA;

        fb_max = 6;              // 74.5 from 7                      // for vga and uxga
        config.jpeg_quality = 6; // 74.5 from 7

        config.fb_count = fb_max + 1;

        // camera init
        cam_err = esp_camera_init(&config);
        if (cam_err != ESP_OK)
        {
            Serial.printf("Camera init failed with error 0x%x", cam_err);
            major_fail();
        }

        new_config = 2;
    }

    delay(100);

    sensor_t *ss = esp_camera_sensor_get();
    ss->set_quality(ss, quality);
    ss->set_framesize(ss, (framesize_t)framesize);
    if (gray == 1)
    {
        ss->set_special_effect(ss, 2); // 0 regular, 2 grayscale
    }
    else
    {
        ss->set_special_effect(ss, 0); // 0 regular, 2 grayscale
    }
    ss->set_brightness(ss, 1);  // up the blightness just a bit
    ss->set_saturation(ss, -2); // lower the saturation

    for (int j = 0; j < 5; j++)
    {
        do_fb(); // start the camera ... warm it up
        delay(50);
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// start_avi - open the files and write in headers
//

static void start_avi()
{

    Serial.println("Starting an avi ");

    SD_MMC.mkdir(strftime_buf2);

    count++;

    sprintf(fname, "/sdcard/%dtestedouglas%d.avi", count, millis());

    Serial.print("\nFile name will be >");
    Serial.print(fname);
    Serial.println("<");

    avifile = fopen(fname, "w");
    idxfile = fopen("/sdcard/idx.tmp", "w");

    if (avifile == NULL)
    {
        Serial.println("Could not open file");
        major_fail();
    }

    if (idxfile == NULL)
    {
        Serial.println("Could not open file");
        major_fail();
    }

    for (i = 0; i < AVIOFFSET; i++)
    {
        char ch = pgm_read_byte(&avi_header[i]);
        buf[i] = ch;
    }

    size_t err = fwrite(buf, 1, AVIOFFSET, avifile);
    // v99 - uxga 13, hd 11, svga 9, vga 8, cif 6
    if (framesize == 8)
    {

        fseek(avifile, 0x40, SEEK_SET);
        err = fwrite(vga_w, 1, 2, avifile);
        fseek(avifile, 0xA8, SEEK_SET);
        err = fwrite(vga_w, 1, 2, avifile);
        fseek(avifile, 0x44, SEEK_SET);
        err = fwrite(vga_h, 1, 2, avifile);
        fseek(avifile, 0xAC, SEEK_SET);
        err = fwrite(vga_h, 1, 2, avifile);
    }
    else if (framesize == 13)
    {

        fseek(avifile, 0x40, SEEK_SET);
        err = fwrite(uxga_w, 1, 2, avifile);
        fseek(avifile, 0xA8, SEEK_SET);
        err = fwrite(uxga_w, 1, 2, avifile);
        fseek(avifile, 0x44, SEEK_SET);
        err = fwrite(uxga_h, 1, 2, avifile);
        fseek(avifile, 0xAC, SEEK_SET);
        err = fwrite(uxga_h, 1, 2, avifile);
    }
    else if (framesize == 11)
    {

        fseek(avifile, 0x40, SEEK_SET);
        err = fwrite(hd_w, 1, 2, avifile);
        fseek(avifile, 0xA8, SEEK_SET);
        err = fwrite(hd_w, 1, 2, avifile);
        fseek(avifile, 0x44, SEEK_SET);
        err = fwrite(hd_h, 1, 2, avifile);
        fseek(avifile, 0xAC, SEEK_SET);
        err = fwrite(hd_h, 1, 2, avifile);
    }
    else if (framesize == 9)
    {

        fseek(avifile, 0x40, SEEK_SET);
        err = fwrite(svga_w, 1, 2, avifile);
        fseek(avifile, 0xA8, SEEK_SET);
        err = fwrite(svga_w, 1, 2, avifile);
        fseek(avifile, 0x44, SEEK_SET);
        err = fwrite(svga_h, 1, 2, avifile);
        fseek(avifile, 0xAC, SEEK_SET);
        err = fwrite(svga_h, 1, 2, avifile);
    }
    else if (framesize == 6)
    {

        fseek(avifile, 0x40, SEEK_SET);
        err = fwrite(cif_w, 1, 2, avifile);
        fseek(avifile, 0xA8, SEEK_SET);
        err = fwrite(cif_w, 1, 2, avifile);
        fseek(avifile, 0x44, SEEK_SET);
        err = fwrite(cif_h, 1, 2, avifile);
        fseek(avifile, 0xAC, SEEK_SET);
        err = fwrite(cif_h, 1, 2, avifile);
    }

    fseek(avifile, AVIOFFSET, SEEK_SET);

    Serial.print(F("\nRecording "));
    Serial.print(total_frames);
    Serial.println(F(" video frames ...\n"));

    startms = millis();
    totalp = 0;
    totalw = 0;
    jpeg_size = 0;
    movi_size = 0;
    uVideoLen = 0;
    idx_offset = 4;

    frame_cnt = 0;
    frames_so_far = 0;

    skipping = 0;
    skipped = 0;
    bad_jpg = 0;
    extend_jpg = 0;
    normal_jpg = 0;

    newfile = 1;
    other_cpu_active = 1;

} // end of start avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  another_save_avi runs on cpu 1, saves another frame to the avi file
//
//  the "baton" semaphore makes sure that only one cpu is using the camera subsystem at a time
//

static void another_save_avi()
{

    xSemaphoreTake(baton, portMAX_DELAY);

    if (fb_in == fb_out)
    { // nothing to do

        xSemaphoreGive(baton);
        nothing_avi++;
    }
    else
    {

        fb_out = (fb_out + 1) % fb_max;

        int fblen;
        fblen = fb_q[fb_out]->len;

        if (BlinkWithWrite)
        {
            digitalWrite(33, LOW);
        }

        jpeg_size = fblen;
        movi_size += jpeg_size;
        uVideoLen += jpeg_size;

        bw = millis();
        size_t dc_err = fwrite(dc_buf, 1, 4, avifile);
        size_t ze_err = fwrite(zero_buf, 1, 4, avifile);

        int time_to_give_up = 0;
        while (ESP.getFreeHeap() < 35000)
        {
            Serial.print(time_to_give_up);
            Serial.print(" Low on heap ");
            Serial.print(ESP.getFreeHeap());
            Serial.print(" frame q = ");
            Serial.println((fb_in + fb_max - fb_out) % fb_max);
            if (time_to_give_up++ == 50)
                break;
            delay(100 + 5 * time_to_give_up);
        }

        size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);

        time_to_give_up = 0;
        while (err != fb_q[fb_out]->len)
        {
            Serial.print("Error on avi write: err = ");
            Serial.print(err);
            Serial.print(" len = ");
            Serial.println(fb_q[fb_out]->len);
            time_to_give_up++;
            if (time_to_give_up == 10)
                major_fail();
            Serial.print(time_to_give_up);
            Serial.print(" Low on heap !!! ");
            Serial.println(ESP.getFreeHeap());

            delay(1000);
            size_t err = fwrite(fb_q[fb_out]->buf, 1, fb_q[fb_out]->len, avifile);
        }

        esp_camera_fb_return(fb_q[fb_out]); // release that buffer back to the camera system
        xSemaphoreGive(baton);

        remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

        print_quartet(idx_offset, idxfile);
        print_quartet(jpeg_size, idxfile);

        idx_offset = idx_offset + jpeg_size + remnant + 8;

        jpeg_size = jpeg_size + remnant;
        movi_size = movi_size + remnant;
        if (remnant > 0)
        {
            size_t rem_err = fwrite(zero_buf, 1, remnant, avifile);
        }

        fileposition = ftell(avifile);                          // Here, we are at end of chunk (after padding)
        fseek(avifile, fileposition - jpeg_size - 4, SEEK_SET); // Here we are the the 4-bytes blank placeholder

        print_quartet(jpeg_size, avifile); // Overwrite placeholder with actual frame size (without padding)

        fileposition = ftell(avifile);

        fseek(avifile, fileposition + jpeg_size, SEEK_SET);

        totalw = totalw + millis() - bw;

        digitalWrite(33, HIGH);
    }
} // end of another_pic_avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  end_avi runs on cpu 1, empties the queue of frames, writes the index, and closes the files
//

static void end_avi()
{

    unsigned long current_end = 0;

    other_cpu_active = 0; // shuts down the picture taking program

    for (int i = 0; i < fb_max; i++)
    { // clear the queue
        another_save_avi();
    }
    current_end = ftell(avifile);

    Serial.println("End of avi - closing the files");

    elapsedms = millis() - startms;
    float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * xspeed;
    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS);
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    // Modify the MJPEG header from the beginning of the file, overwriting various placeholders

    fseek(avifile, 4, SEEK_SET);
    print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, avifile);

    fseek(avifile, 0x20, SEEK_SET);
    print_quartet(us_per_frame, avifile);

    unsigned long max_bytes_per_sec = movi_size * iAttainedFPS / frame_cnt;

    fseek(avifile, 0x24, SEEK_SET);
    print_quartet(max_bytes_per_sec, avifile);

    fseek(avifile, 0x30, SEEK_SET);
    print_quartet(frame_cnt, avifile);

    fseek(avifile, 0x8c, SEEK_SET);
    print_quartet(frame_cnt, avifile);

    fseek(avifile, 0x84, SEEK_SET);
    print_quartet((int)iAttainedFPS, avifile);

    fseek(avifile, 0xe8, SEEK_SET);
    print_quartet(movi_size + frame_cnt * 8 + 4, avifile);

    Serial.println(F("\n*** Video recorded and saved ***\n"));
    Serial.print(F("Recorded "));
    Serial.print(elapsedms / 1000);
    Serial.print(F("s in "));
    Serial.print(frame_cnt);
    Serial.print(F(" frames\nFile size is "));
    Serial.print(movi_size + 12 * frame_cnt + 4);
    Serial.print(F(" bytes\nActual FPS is "));
    Serial.print(fRealFPS, 2);
    Serial.print(F("\nMax data rate is "));
    Serial.print(max_bytes_per_sec);
    Serial.print(F(" byte/s\nFrame duration is "));
    Serial.print(us_per_frame);
    Serial.println(F(" us"));
    Serial.print(F("Average frame length is "));
    Serial.print(uVideoLen / frame_cnt);
    Serial.println(F(" bytes"));
    Serial.print("Average picture time (ms) ");
    Serial.println(1.0 * totalp / frame_cnt);
    Serial.print("Average write time (ms)   ");
    Serial.println(totalw / frame_cnt);
    Serial.print("Frames Skipped % ");
    Serial.println(100.0 * skipped / frame_cnt, 2);
    Serial.print("Normal jpg % ");
    Serial.println(100.0 * normal_jpg / frame_cnt, 1);
    Serial.print("Extend jpg % ");
    Serial.println(100.0 * extend_jpg / frame_cnt, 1);
    Serial.print("Bad    jpg % ");
    Serial.println(100.0 * bad_jpg / total_frames, 1);

    Serial.println("Writing the index");

    fseek(avifile, current_end, SEEK_SET);

    fclose(idxfile);

    size_t i1_err = fwrite(idx1_buf, 1, 4, avifile);

    print_quartet(frame_cnt * 16, avifile);

    idxfile = fopen("/sdcard/idx.tmp", "r");

    if (idxfile != NULL)
    {
        Serial.printf("File open: %s\n", "/sdcard/idx.tmp");
    }
    else
    {
        Serial.println("Could not open file");
    }

    char *AteBytes;
    AteBytes = (char *)malloc(8);

    for (int i = 0; i < frame_cnt; i++)
    {
        size_t res = fread(AteBytes, 1, 8, idxfile);
        size_t i1_err = fwrite(dc_buf, 1, 4, avifile);
        size_t i2_err = fwrite(zero_buf, 1, 4, avifile);
        size_t i3_err = fwrite(AteBytes, 1, 8, avifile);
    }

    free(AteBytes);
    fclose(idxfile);
    fclose(avifile);
    int xx = remove("/sdcard/idx.tmp");

    Serial.println("---");
}

static void do_fb()
{
    xSemaphoreTake(baton, portMAX_DELAY);
    camera_fb_t *fb = esp_camera_fb_get();

    // Serial.print("Pic, len="); Serial.println(fb->len);

    esp_camera_fb_return(fb);
    xSemaphoreGive(baton);
}

////////////////////////////////////////////////////////////////////////////////////
//
// some globals for the loop()
//

void stop_handler()
{
    recording = 0;
    Serial.println("stopping recording");

    xTaskNotifyGive(AviWriterTask);
}

void start_handler()
{
    char buf[120];
    size_t buf_len;
    char new_res[20];

    if (recording == 1)
    {
        const char *resp = "You must Stop recording, before starting a new one.  Start over ...";
    }
    else
    {
        // recording = 1;
        Serial.println("starting recording");

        framesize = 8;
        capture_interval = 100;
        total_frames_config = 18000;
        xlength = total_frames_config * capture_interval / 1000;
        repeat_config = 100;
        quality = 12;
        xspeed = 1;
        gray = 0;
        config_camera();

        recording = 1;
        xTaskNotifyGive(AviWriterTask);
    }
}
