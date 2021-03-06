#pragma once

#pragma once

#include <stdio.h>
#include <cstdlib>
#include <queue>
#include "al.h"
#include "alc.h"

#include "main.h"

extern "C" {
#include "libavformat/avformat.h"	//封装格式
#include "libavcodec/avcodec.h"	//解码
#include "libswresample/swresample.h"
};


#define MAX_AUDIO_FARME_SIZE 48000 * 2
#define NUMBUFFERS (4)
#define VOLUME_UP 0.2
#define VOLUME_DOWN -0.2


typedef struct _tFrame {
    void* data;
    int size;
    int samplerate;
    double pts;
}TFRAME, * PTFRAME;


class AudioPlayer {
public:
    AudioPlayer(char* filepath, player_stat_t* is);
    ~AudioPlayer();
    int decode();
    int audio_playing();
    void adjustVolume(double v);

private:
    int OpenAL_init();
    int SoundCallback(ALuint& bufferID);
    int Play();
    int audio_play_thread();
    void forward_func(int second);

public:
    player_stat_t* is;

private:
    std::shared_ptr<std::thread> m_pAudio;

    AVFormatContext* pFormatCtx; //解封装
    AVCodecContext* pCodecCtx; //解码
    AVCodec* pCodec;
    AVFrame* pFrame, * pFrameYUV; //帧数据
    AVPacket* packet;	//解码前的压缩数据（包数据）
    int index; //编码器索引位置
    uint8_t* out_buffer;	//数据缓冲区
    int out_buffer_size;    //缓冲区大小
    SwrContext* swrCtx;

    enum AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    int out_channel_nb;


    std::queue<PTFRAME> queueData; //保存解码后数据
    ALuint m_source;
    ALuint m_buffers[NUMBUFFERS];

    double volume;    //音量
};


