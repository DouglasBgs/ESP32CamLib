
#include <dummy.h>
#include <stdio.h>
#include <SD_MMC.h>

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

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/cpu.h"
#include "soc/rtc_cntl_reg.h"

#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "nvs.h"

void codeForAviWriterTask(void *parameter);
void codeForCameraTask(void *parameter);
static void inline print_quartet(unsigned long i, FILE *fd);
void delete_old_stuff();
void deleteFolderOrFile(const char *val);
void init_tasks();
void major_fail();
static esp_err_t init_sdcard();
void make_avi();
static void config_camera();
static void start_avi();
static void another_save_avi();
static void end_avi();
static void do_fb();
void stop_handler();
void start_handler(char *file);
