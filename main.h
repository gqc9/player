#pragma once

#include <stdio.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <cstdlib>
#include <queue>
#include "al.h"
#include "alc.h"



extern "C" {
#include "libavformat/avformat.h"	//��װ��ʽ
#include "libavcodec/avcodec.h"	//����
#include "libswscale/swscale.h"	//����
#include "libswresample/swresample.h" //�ز���
#include "libavutil/imgutils.h" //����ͼ��ʹ��
#include "libavutil/time.h"
#include "SDL2/SDL.h"	//����
};


/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0



#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
//#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))
#define FRAME_QUEUE_SIZE SUBPICTURE_QUEUE_SIZE


typedef struct {
    AVFrame* frame;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;                    // frame��Ӧ��packet�������ļ��еĵ�ַƫ��
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
}   frame_t;


typedef struct {
    frame_t queue[FRAME_QUEUE_SIZE];
    int rindex;                     // ��������������ʱ��ȡ��֡���в��ţ����ź��֡��Ϊ��һ֡
    int windex;                     // д����
    int size;                       // ��֡��
    int max_size;                   // ���пɴ洢���֡��
    int keep_last;
    int rindex_shown;               // ��ǰ�Ƿ���֡����ʾ
    SDL_mutex* mutex;
    SDL_cond* cond;
    //packet_queue_t* pktq;           // ָ���Ӧ��packet_queue
}   frame_queue_t;


typedef struct {
    double pts;                     // ��ǰ֡(������)��ʾʱ��������ź󣬵�ǰ֡�����һ֡
    double pts_drift;               // ��ǰ֡��ʾʱ����뵱ǰϵͳʱ��ʱ��Ĳ�ֵ
    double last_updated;            // ��ǰʱ��(����Ƶʱ��)���һ�θ���ʱ�䣬Ҳ�ɳƵ�ǰʱ��ʱ��
    double speed;                   // ʱ���ٶȿ��ƣ����ڿ��Ʋ����ٶ�
    //int serial;                     // �������У���ν�������о���һ�������Ĳ��Ŷ�����һ��seek����������һ���µĲ�������
    int paused;                     // ��ͣ��־
    //int* queue_serial;              // ָ��packet_serial
}   play_clock_t;


typedef struct { 
    play_clock_t audio_clk;                   // ��Ƶʱ��
    play_clock_t video_clk;                   // ��Ƶʱ��
    double frame_timer;

    //packet_queue_t audio_pkt_queue;
    //packet_queue_t video_pkt_queue;
    //frame_queue_t audio_frm_queue;
    //frame_queue_t video_frm_queue;

    //struct SwsContext* img_convert_ctx;
    //struct SwrContext* audio_swr_ctx;
    //AVFrame* p_frm_yuv;

    //audio_param_t audio_param_src;
    //audio_param_t audio_param_tgt;
    //int audio_hw_buf_size;              // SDL��Ƶ��������С(��λ�ֽ�)
    //uint8_t* p_audio_frm;               // ָ������ŵ�һ֡��Ƶ���ݣ�ָ�����������������SDL��Ƶ���������������ز�����ָ��audio_frm_rwr������ָ��frame�е���Ƶ
    //uint8_t* audio_frm_rwr;             // ��Ƶ�ز��������������
    //unsigned int audio_frm_size;        // �����ŵ�һ֡��Ƶ����(audio_bufָ��)�Ĵ�С
    //unsigned int audio_frm_rwr_size;    // ���뵽����Ƶ������audio_frm_rwr��ʵ�ʳߴ�
    //int audio_cp_index;                 // ��ǰ��Ƶ֡���ѿ���SDL��Ƶ��������λ������(ָ���һ���������ֽ�)
    //int audio_write_buf_size;           // ��ǰ��Ƶ֡����δ����SDL��Ƶ����������������audio_frm_size = audio_cp_index + audio_write_buf_size
    double audio_clock;
    //int audio_clock_serial;

    //int abort_request;
    //int paused;
    //int step;

    //SDL_cond* continue_read_thread;
    //SDL_Thread* read_tid;           // demux�⸴���߳�

}   player_stat_t;



double get_clock(play_clock_t* c);
void set_clock_at(play_clock_t* c, double pts, /*int serial, */double time);
void set_clock(play_clock_t* c, double pts);