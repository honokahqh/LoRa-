#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "shine_mp3.h"
#include "minimp3.h"
//  ��ƵMP3����ֻ��һ������:
//  pcm:8KHz 16bit��������һ֡����576 samples
//  MP3:8kbps  һ֡����72 bytes
extern int32_t Sys_ms;

MP3Encoder encoder;               // MP3������

/* Output error message and exit */
void error(char *s) {
    fprintf(stderr, "Error: %s\n", s);
    exit(1);
}

// Initialize the MP3 encoder
void init_mp3_encoder(MP3Encoder* encoder) {
    encoder->config.wave.samplerate = 8000; // 8KHz
    encoder->config.wave.channels = 1; // Mono
    encoder->config.mpeg.bitr = 8;
    encoder->config.mpeg.copyright = 0;
    encoder->config.mpeg.emph = NONE;
    encoder->config.mpeg.original = 1;

    if (shine_check_config(encoder->config.wave.samplerate, encoder->config.mpeg.bitr) < 0)
        error("Unsupported samplerate/bitrate configuration.");

    encoder->config.mpeg.mode = MONO;

    encoder->s = shine_initialise(&encoder->config);
}

// Clean up the MP3 encoder

void deinit_mp3_encoder(MP3Encoder* encoder) {
    shine_close(encoder->s);
}

// Encode the input data to MP3:Only one frame
uint8_t encode_to_mp3(MP3Encoder* encoder, int16_t *data_in, uint8_t *data_out) {
    unsigned char *data;
    int written;
//    int samples_per_pass = shine_samples_per_pass(encoder->s) * encoder->config.wave.channels;
    data = shine_encode_buffer_interleaved(encoder->s, data_in, &written);
    if(written == 72)
        memcpy(data_out, data, written);
    shine_flush(encoder->s, &written);
    return (uint8_t)written;
}

mp3dec_t mp3d;
void init_mp3_decoder()
{
    mp3dec_init(&mp3d);
}

uint16_t decode_to_mp3_32kbps(uint8_t *data_in, short *data_out)
{
    mp3dec_frame_info_t info;
    return mp3dec_decode_frame(&mp3d, data_in, 144, data_out, &info);
}

uint16_t decode_to_mp3_8kbps(uint8_t *data_in, short *data_out)
{
    mp3dec_frame_info_t info;
    return mp3dec_decode_frame(&mp3d, data_in, 72, data_out, &info);
}

void _exit(int status) {
    (void) status;
    while(1) {};  // �˴�����
}

extern char end asm("end");
static char *heap_end;

void *_sbrk(int incr) {
    char *prev_heap_end;
    if (heap_end == 0) {
        heap_end = &end;
    }
    prev_heap_end = heap_end;
    heap_end += incr;
    return (void *) prev_heap_end;
}

ssize_t _write(int file, const char *ptr, size_t len) {
    // ʵ��д�뵽UART������ͨ�Žӿ�
    // ���ڣ��򵥵ض������
    (void) file;
    (void) ptr;
    return len;
}

int _close(int file) {
    return -1; // ����ʧ��
}

struct stat {
    int st_mode;
};

#define S_IFCHR 0020000  // �ַ��豸

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

ssize_t _read(int file, char *ptr, size_t len) {
    return 0;
}

int _kill(int pid, int sig) {
    (void) pid;
    (void) sig;
    return -1;
}

int _getpid(void) {
    return 1;
}