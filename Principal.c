// ============= Bibliotecas e Definições =============
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/rtc.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"

#include "ssd1306.h"
#include "font.h"

#include "ff.h"
#include "f_util.h"
#include "sd_card.h"
#include "hw_config.h"
#include "my_debug.h"

// ============= Definições de Pinos =============
// I2C0 - MPU6050
#define I2C_MPU_PORT i2c0
#define I2C_MPU_SDA 0
#define I2C_MPU_SCL 1

// I2C1 - Display OLED
#define I2C_OLED_PORT i2c1
#define I2C_OLED_SDA 14
#define I2C_OLED_SCL 15
#define ENDERECO_OLED 0x3C

// Botões
#define BOTAO_GRAVAR 5
#define BOTAO_SD 22
#define BOTAO_BOOTSEL 6

// MPU6050
#define MPU6050_ADDR 0x68

// Buzzer
#define BUZZER 21

#define BEEP_DURATION_MS 100
#define BEEP_INTERVAL_MS 200

// Estados internos do buzzer
static bool buzzer_on_flag = false; // buzzer ativo ou não
static bool buzzer_active = false;  // buzzer tocando (PWM ligado)
static int bips_total = 0;          // total de bips para dar
static int bips_count = 0;          // quantos bips já deram
static absolute_time_t buzzer_last_toggle_time;

// LED RGB
#define LED_R 13
#define LED_G 11
#define LED_B 12

// Variaveis de Configuração
int16_t acel[3], giro[3], temp;

float ax = 0.0f;
float ay = 0.0f;
float az = 0.0f;
float roll = 0.0f;
float pitch = 0.0f;

// ============= Struct de Estados =============
typedef struct
{
    bool gravando;
    bool sd_montado;
    uint32_t contador_amostras;
} EstadoSistema;

typedef enum
{
    LED_INICIALIZANDO,
    LED_PRONTO,
    LED_GRAVANDO,
    LED_SD_ACESSO,
    LED_ERRO
} EstadosLed;

struct EstadosLed
{
    EstadosLed estado;
};

// ============= Prototipagem de Funções =============
void mpu6050_reset();
void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp);
void atualizar_display(float roll, float pitch, ssd1306_t *ssd, EstadoSistema *sistema, const char *status);
void salvar_dado_csv(FIL *file, uint32_t amostra, int16_t a[3], int16_t g[3]);
void controlar_leds(struct EstadosLed estado);
void atualizar_leituras();
void buzzer_init(void);
void buzzer_start(int num_bips);
void buzzer_stop(void);
void buzzer_update(void);
void rgb_init(void);

// ============= Funções de Sensor =============
void mpu6050_reset()
{
    uint8_t buf[] = {0x6B, 0x80};
    i2c_write_blocking(I2C_MPU_PORT, MPU6050_ADDR, buf, 2, false);
    sleep_ms(100);
    buf[1] = 0x00;
    i2c_write_blocking(I2C_MPU_PORT, MPU6050_ADDR, buf, 2, false);
    sleep_ms(10);
}

void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp)
{
    uint8_t buffer[6], reg;

    reg = 0x3B;
    i2c_write_blocking(I2C_MPU_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_MPU_PORT, MPU6050_ADDR, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        accel[i] = (buffer[i * 2] << 8) | buffer[i * 2 + 1];

    reg = 0x43;
    i2c_write_blocking(I2C_MPU_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_MPU_PORT, MPU6050_ADDR, buffer, 6, false);
    for (int i = 0; i < 3; i++)
        gyro[i] = (buffer[i * 2] << 8) | buffer[i * 2 + 1];

    reg = 0x41;
    i2c_write_blocking(I2C_MPU_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_MPU_PORT, MPU6050_ADDR, buffer, 2, false);
    *temp = (buffer[0] << 8) | buffer[1];
}

// ============= Display OLED =============
void atualizar_display(float roll, float pitch, ssd1306_t *ssd, EstadoSistema *sistema, const char *status)
{
    char str_roll[16], str_pitch[16], str_amostra[16];
    snprintf(str_roll, sizeof(str_roll), "Roll: %5.1f", roll);
    snprintf(str_pitch, sizeof(str_pitch), "Pitch: %5.1f", pitch);
    snprintf(str_amostra, sizeof(str_amostra), "Amostra: %lu", sistema->contador_amostras);

    ssd1306_fill(ssd, false);
    ssd1306_draw_string(ssd, status, 0, 0);       // 1ª linha
    ssd1306_draw_string(ssd, "", 0, 12);          // 2ª linha (em branco)
    ssd1306_draw_string(ssd, str_amostra, 0, 24); // 3ª linha
    ssd1306_draw_string(ssd, str_roll, 0, 36);    // 4ª linha
    ssd1306_draw_string(ssd, str_pitch, 0, 48);   // 5ª linha
    ssd1306_send_data(ssd);
}

// ============= Salvar no CSV =============
void salvar_dado_csv(FIL *file, uint32_t amostra, int16_t a[3], int16_t g[3])
{
    char linha[100];
    snprintf(linha, sizeof(linha), "%lu;%d;%d;%d;%d;%d;%d\r\n",
             amostra, a[0], a[1], a[2], g[0], g[1], g[2]);
    f_puts(linha, file);
}

// ============= LED  =============
void controlar_leds(struct EstadosLed estado)
{
    uint slice_r = pwm_gpio_to_slice_num(LED_R);
    uint slice_g = pwm_gpio_to_slice_num(LED_G);
    uint slice_b = pwm_gpio_to_slice_num(LED_B);
    uint chan_r = pwm_gpio_to_channel(LED_R);
    uint chan_g = pwm_gpio_to_channel(LED_G);
    uint chan_b = pwm_gpio_to_channel(LED_B);

    switch (estado.estado)
    {
    case LED_INICIALIZANDO:
        pwm_set_chan_level(slice_r, chan_r, 128);
        pwm_set_chan_level(slice_g, chan_g, 128);
        pwm_set_chan_level(slice_b, chan_b, 0);
        printf("[ALERTA] LED AMARELO ativado.\n");
        break;
    case LED_PRONTO:
        pwm_set_chan_level(slice_r, chan_r, 0);
        pwm_set_chan_level(slice_g, chan_g, 255);
        pwm_set_chan_level(slice_b, chan_b, 0);
        printf("[ALERTA] LED VERDE ativado.\n");
        break;
    case LED_GRAVANDO:
        pwm_set_chan_level(slice_r, chan_r, 255);
        pwm_set_chan_level(slice_g, chan_g, 0);
        pwm_set_chan_level(slice_b, chan_b, 0);
        printf("[ALERTA] LED VERMELHO ativado.\n");
        break;
    case LED_SD_ACESSO:
        pwm_set_chan_level(slice_r, chan_r, 0);
        pwm_set_chan_level(slice_g, chan_g, 0);
        pwm_set_chan_level(slice_b, chan_b, 255);
        printf("[ALERTA] LED AZUL ativado.\n");
        break;
    case LED_ERRO:
        pwm_set_chan_level(slice_r, chan_r, 128);
        pwm_set_chan_level(slice_g, chan_g, 0);
        pwm_set_chan_level(slice_b, chan_b, 128);
        printf("[ALERTA] LED ROXO ativado.\n");
        buzzer_start(2); // 2 bips curtos ao indicar erro
        break;
    }
}

void atualizar_leituras()
{
    mpu6050_read_raw(acel, giro, &temp);

    ax = acel[0] / 16384.0f;
    ay = acel[1] / 16384.0f;
    az = acel[2] / 16384.0f;
    roll = atan2(ay, az) * 180.0f / M_PI;
    pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;
}

// ============= Buzzer =============
// Inicializa hardware do buzzer (chamar uma vez)
void buzzer_init(void)
{
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    uint freq = 2000; // frequência do beep
    uint top = 125000000 / freq;
    pwm_set_wrap(slice, top);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(BUZZER), (top * 8) / 10);
    pwm_set_enabled(slice, false);
    buzzer_last_toggle_time = get_absolute_time();
}

// Começa a sequência de bips (não bloqueante)
void buzzer_start(int num_bips)
{
    if (num_bips <= 0)
    {
        buzzer_stop();
        return;
    }
    buzzer_on_flag = true;
    buzzer_active = false;
    bips_total = num_bips;
    bips_count = 0;
    buzzer_last_toggle_time = get_absolute_time();
}

// Para a sequência do buzzer imediatamente
void buzzer_stop(void)
{
    buzzer_on_flag = false;
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_enabled(slice, false);
    buzzer_active = false;
    bips_total = 0;
    bips_count = 0;
}

// Atualiza o estado do buzzer
void buzzer_update(void)
{
    if (!buzzer_on_flag)
        return;

    absolute_time_t now = get_absolute_time();
    int32_t elapsed = absolute_time_diff_us(buzzer_last_toggle_time, now) / 1000; // ms
    uint slice = pwm_gpio_to_slice_num(BUZZER);

    if (buzzer_active)
    {
        // buzzer ligado, checa se deve desligar (fim do beep)
        if (elapsed >= BEEP_DURATION_MS)
        {
            pwm_set_enabled(slice, false);
            buzzer_active = false;
            buzzer_last_toggle_time = now;
            bips_count++;

            // Se já deu todos os bips, para tudo
            if (bips_count >= bips_total)
            {
                buzzer_stop();
            }
        }
    }
    else
    {
        // buzzer desligado, checa se deve ligar novo beep
        if (elapsed >= BEEP_INTERVAL_MS && bips_count < bips_total)
        {
            pwm_set_enabled(slice, true);
            buzzer_active = true;
            buzzer_last_toggle_time = now;
        }
    }
}

// ============= LED RGB =============
void rgb_init(void)
{
    // LED RGB
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_set_function(LED_G, GPIO_FUNC_PWM);
    gpio_set_function(LED_B, GPIO_FUNC_PWM);
    pwm_set_wrap(pwm_gpio_to_slice_num(LED_R), 255);
    pwm_set_wrap(pwm_gpio_to_slice_num(LED_G), 255);
    pwm_set_wrap(pwm_gpio_to_slice_num(LED_B), 255);
    pwm_set_enabled(pwm_gpio_to_slice_num(LED_R), true);
    pwm_set_enabled(pwm_gpio_to_slice_num(LED_G), true);
    pwm_set_enabled(pwm_gpio_to_slice_num(LED_B), true);

    printf("[INIT] RGB inicializado.\n");
}
// ============= Botões e Interrupções =============
volatile bool iniciar_gravacao = false;
volatile bool listar_sd = false;

void gpio_irq_handler(uint gpio, uint32_t events)
{
    if (gpio == BOTAO_GRAVAR && events == GPIO_IRQ_EDGE_FALL)
    {
        iniciar_gravacao = true;
        buzzer_start(3); // 3 bips curtos ao finalizar gravação
    }
    else if (gpio == BOTAO_SD && events == GPIO_IRQ_EDGE_FALL)
    {
        listar_sd = true;
        buzzer_start(1); // 1 bip curto ao listar SD
    }
    else if (gpio == BOTAO_BOOTSEL && events == GPIO_IRQ_EDGE_FALL)
    {
        reset_usb_boot(0, 0);
    }
}

// ============= Função Principal =============
int main()
{
    stdio_init_all();
    printf("Sistema inicializando...\n");
    sleep_ms(5000);

    // I2C - MPU6050
    i2c_init(I2C_MPU_PORT, 400 * 1000);
    gpio_set_function(I2C_MPU_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_MPU_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MPU_SDA);
    gpio_pull_up(I2C_MPU_SCL);
    mpu6050_reset();

    // I2C - Display OLED
    i2c_init(I2C_OLED_PORT, 400 * 1000);
    gpio_set_function(I2C_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_OLED_SDA);
    gpio_pull_up(I2C_OLED_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_OLED, I2C_OLED_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Estado inicial
    struct EstadosLed estado_led = {LED_INICIALIZANDO};
    controlar_leds(estado_led);
    EstadoSistema sistema = {.gravando = false, .sd_montado = false, .contador_amostras = 0};
    atualizar_leituras();
    atualizar_display(roll, pitch, &ssd, &sistema, "Inicializado");
    sleep_ms(500);

    // ==== Botões =============
    // Botão Gravar
    gpio_init(BOTAO_GRAVAR);
    gpio_set_dir(BOTAO_GRAVAR, GPIO_IN);
    gpio_pull_up(BOTAO_GRAVAR);

    // Botão Bootsel
    gpio_init(BOTAO_BOOTSEL);
    gpio_set_dir(BOTAO_BOOTSEL, GPIO_IN);
    gpio_pull_up(BOTAO_BOOTSEL);

    // Botão SD
    gpio_init(BOTAO_SD);
    gpio_set_dir(BOTAO_SD, GPIO_IN);
    gpio_pull_up(BOTAO_SD);

    // Configurar interrupções
    gpio_set_irq_enabled_with_callback(BOTAO_GRAVAR, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_BOOTSEL, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled_with_callback(BOTAO_SD, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // ==== Buzzer ====
    buzzer_init();
    rgb_init();

    // Loop principal
    while (true)
    {
        buzzer_update();
        atualizar_leituras();
        atualizar_display(roll, pitch, &ssd, &sistema, "Pronto para gravar");
        printf("Aguardando ação...\n");
        printf("Roll: %.2f, Pitch: %.2f\n", roll, pitch);

        estado_led.estado = LED_INICIALIZANDO;
        controlar_leds(estado_led);

        if (iniciar_gravacao && !sistema.gravando)
        {
            printf("Botão de gravação pressionado. Iniciando processo...\n");

            // Montar cartão
            printf("Montando cartão SD...\n");
            estado_led.estado = LED_SD_ACESSO;
            controlar_leds(estado_led);
            sleep_ms(500);
            FRESULT fr = f_mount(&sd_get_by_num(0)->fatfs, sd_get_by_num(0)->pcName, 1);
            if (fr != FR_OK)
            {
                printf("Erro ao montar o cartão SD.\n");
                atualizar_leituras();
                atualizar_display(roll, pitch, &ssd, &sistema, "Erro: SD");
                estado_led.estado = LED_ERRO;
                controlar_leds(estado_led);
                sleep_ms(500); // Aguarda antes de tentar novamente
                iniciar_gravacao = false;
                continue;
            }
            printf("Cartão SD montado com sucesso.\n");
            atualizar_leituras();
            atualizar_display(roll, pitch, &ssd, &sistema, "SD Montado");
            estado_led.estado = LED_PRONTO;
            controlar_leds(estado_led);
            sleep_ms(500);

            // Criar arquivo
            printf("Criando arquivo CSV...\n");
            FIL file;
            fr = f_open(&file, "imu_data.csv", FA_WRITE | FA_CREATE_ALWAYS);
            if (fr != FR_OK)
            {
                printf("Erro ao criar arquivo.\n");
                atualizar_leituras();
                atualizar_display(roll, pitch, &ssd, &sistema, "Erro arquivo");
                estado_led.estado = LED_ERRO;
                controlar_leds(estado_led);
                f_unmount(sd_get_by_num(0)->pcName);
                sleep_ms(500); // Aguarda antes de tentar novamente
                iniciar_gravacao = false;
                continue;
            }

            f_puts("numero_amostra;accel_x;accel_y;accel_z;giro_x;giro_y;giro_z\r\n", &file);
            printf("Cabeçalho CSV escrito com sucesso.\n");
            atualizar_leituras();
            atualizar_display(roll, pitch, &ssd, &sistema, "Gravando...");
            estado_led.estado = LED_GRAVANDO;
            controlar_leds(estado_led);
            sistema.contador_amostras = 0;
            sistema.gravando = true;

            // Coleta de dados
            for (int i = 0; i < 30; i++)
            {
                atualizar_leituras();

                sistema.contador_amostras++;
                salvar_dado_csv(&file, sistema.contador_amostras, acel, giro);

                atualizar_display(roll, pitch, &ssd, &sistema, "Gravando...");
                printf("Amostra %lu salva.\n", sistema.contador_amostras);
                sleep_ms(500);
            }

            f_close(&file);
            printf("Arquivo fechado.\n");

            // Desmontar cartão
            printf("Desmontando cartão SD...\n");
            f_unmount(sd_get_by_num(0)->pcName);
            printf("Cartão SD desmontado.\n");

            atualizar_display(roll, pitch, &ssd, &sistema, "Dados Salvos!");

            estado_led.estado = LED_PRONTO;
            controlar_leds(estado_led);
            sistema.gravando = false;
            iniciar_gravacao = false;
        }
        else if (listar_sd)
        {
            printf("Listar SD solicitado. Montando cartão...\n");

            // Monta o SD
            FRESULT fr = f_mount(&sd_get_by_num(0)->fatfs, sd_get_by_num(0)->pcName, 1);
            if (fr != FR_OK)
            {
                printf("Erro ao montar cartão SD para listagem.\n");
                atualizar_display(0, 0, &ssd, &sistema, "Erro: SD");
                estado_led.estado = LED_ERRO;
                controlar_leds(estado_led);
                sleep_ms(500); // Aguarda antes de tentar novamente
                listar_sd = false;
                continue;
            }

            atualizar_display(0, 0, &ssd, &sistema, "Listar SD");
            printf("Cartão SD montado. Listando arquivos:\n");

            // Lista arquivos na raiz
            DIR dir;
            FILINFO fno;
            FRESULT res = f_opendir(&dir, "/");
            if (res == FR_OK)
            {
                while (true)
                {
                    res = f_readdir(&dir, &fno);
                    if (res != FR_OK || fno.fname[0] == 0)
                        break;

                    if (fno.fattrib & AM_DIR)
                    {
                        printf("[DIR]  %s\n", fno.fname);
                    }
                    else
                    {
                        printf("[FILE] %s (%lu bytes)\n", fno.fname, (DWORD)fno.fsize);
                    }
                }
                f_closedir(&dir);
            }
            else
            {
                printf("Erro ao abrir diretório raiz: %d\n", res);
            }

            sleep_ms(50); // Aguarda antes de desmontar

            // Desmonta o SD
            f_unmount(sd_get_by_num(0)->pcName);
            printf("Cartão SD desmontado após listagem.\n");
            atualizar_display(0, 0, &ssd, &sistema, "Listagem OK");
            estado_led.estado = LED_PRONTO;
            controlar_leds(estado_led);

            listar_sd = false;
        }

        sleep_ms(100); // idle delay
    }

    return 0;
}
