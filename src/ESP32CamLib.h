#ifndef ESP32CAMLIB_H
#define ESP32CAMLIB_H

#include <Arduino.h>

class ESP32CamLib
{
public:
    void codeForAviWriterTask(void *parameter);
    void codeForCameraTask(void *parameter);
    void inline print_quartet(unsigned long i, FILE *fd);
    void delete_old_stuff();
    void deleteFolderOrFile(const char *val);
    void init_tasks();
    void major_fail();
    static esp_err_t init_sdcard();
    void make_avi();
    void config_camera();
    void start_avi();
    void another_save_avi();
    void end_avi();
    void do_fb();
    static esp_err_t start_handler();
    static esp_err_t stop_handler();
};

#endif