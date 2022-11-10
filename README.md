# ESP32CamLib

## Introdução

Biblioteca para gravação de vídeo através da câmera utilizando uma placa ESP32-CAM.

## Como iniciar

1. Faça o download do projeto e abra-o no Arduino IDE.
`gh repo clone DouglasBgs/ESP32CamLib` ou `https://github.com/DouglasBgs/ESP32CamLib.git`

2. Abra o arquivo `settings.h` e configure as variáveis de acordo com a sua necessidade.
3. Importe a biblioteca `ESP32CamLib` para o seu projeto através da sua IDE de preferência, *ArduinoIDE* ou *PlatformIO*.

4. Utilize as funções `void sart_handler()` e `stop_handler()` para iniciar e parar a gravação do vídeo. Neste processo a biblioteca já grava o vídeo diretamente no cartão sd utlizando um nome já pré defido.


Outras Funções Disponíveis:
```c++
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
```

### Este projeto é baseado em um projeto de [**@jameszah**](https://github.com/jameszah/)

>https://github.com/jameszah/ESP32-CAM-Video-Recorder
>
>jameszah/ESP32-CAM-Video-Recorder is licensed under the
>GNU General Public License v3.0
>
