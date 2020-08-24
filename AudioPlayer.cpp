#include "AudioPlayer.h"


int AudioPlayer::OpenAL_init() {
    ALCdevice* pDevice;
    ALCcontext* pContext;

    pDevice = alcOpenDevice(NULL);
    pContext = alcCreateContext(pDevice, NULL);
    alcMakeContextCurrent(pContext);

    if (alcGetError(pDevice) != ALC_NO_ERROR)
        return AL_FALSE;

    return 0;
}


int AudioPlayer::SoundCallback(ALuint& bufferID) {
    if (queueData.empty()) return -1;
    PTFRAME frame = queueData.front();
    queueData.pop();
    if (frame == nullptr)
        return -1;
    //������д��buffer
    alBufferData(bufferID, AL_FORMAT_STEREO16, frame->data, frame->size, frame->samplerate);
    //��buffer�Żػ�����
    alSourceQueueBuffers(m_source, 1, &bufferID);
    //�ͷ�����
    if (frame) {
        av_free(frame->data);
        delete frame;
    }
    int bytes_per_sec = av_samples_get_buffer_size(NULL, out_channel_nb, out_sample_rate, out_sample_fmt, 1);
    //����ʱ��
    if (!isnan(is->audio_clock)) {
        // ������Ƶʱ�ӣ�����ʱ�̣�ÿ���������������������ݺ�
        // ǰ��audio_decode_frame�и��µ�is->audio_clock������Ƶ֡Ϊ��λ�����Դ˴��ڶ�������Ҫ��ȥδ����������ռ�õ�ʱ��
        set_clock_at(&is->audio_clk,
            //is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_param_tgt.bytes_per_sec,
            is->audio_clock,
            //is->audio_clock_serial,
            av_gettime_relative() / 1000000.0);
    }

    return 0;
}

int AudioPlayer::Play() {
    int state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_STOPPED || state == AL_INITIAL) {
        alSourcePlay(m_source);
    }
    return 0;
}


int AudioPlayer::audio_player_init(char* filepath) {
    av_register_all();	//ע���
    avformat_network_init();
    avcodec_register_all();
    pFormatCtx = avformat_alloc_context();

    //����Ƶ�ļ�����ʼ��pFormatCtx
    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    //��ȡ�ļ���Ϣ
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    //��ȡ����ý�����ı�������Ϣ���ҵ���Ӧ��type���ڵ�pFormatCtx->streams������λ�ã���ʼ����������������Ƶʱtype��AUDIO
    index = -1;
    for (int i = 0; i<pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            index = i;
            break;
        }
    if (index == -1) {
        printf("Didn't find a audio stream.\n");
        return -1;
    }
    //��ȡ������
    pCodec = avcodec_find_decoder(pFormatCtx->streams[index]->codecpar->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[index]->codecpar);
    pCodecCtx->pkt_timebase = pFormatCtx->streams[index]->time_base;
    //�򿪽�����
    if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        printf("Couldn't open codec.\n");
        return -1;
    }

    //�ڴ����
    packet = (AVPacket*)av_malloc(sizeof(AVPacket));
    pFrame = av_frame_alloc();
    swrCtx = swr_alloc();

    //���ò������� frame->16bit˫���� ������44100 PCM������ʽ   
    enum AVSampleFormat in_sample_fmt = pCodecCtx->sample_fmt;  //����Ĳ�����ʽ  
    out_sample_fmt = AV_SAMPLE_FMT_S16; //���������ʽ16bit PCM  
    int in_sample_rate = pCodecCtx->sample_rate; //���������
    out_sample_rate = in_sample_rate; //���������  
    uint64_t in_ch_layout = pCodecCtx->channel_layout; //�������������   
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO; //������������֣���������
    swr_alloc_set_opts(swrCtx,
        out_ch_layout, out_sample_fmt, out_sample_rate,
        in_ch_layout, in_sample_fmt, in_sample_rate,
        0, NULL); //���ò���
    swr_init(swrCtx); //��ʼ��

    //�����������ֻ�ȡ�������������
    out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);

   
}


int AudioPlayer::decode() {
    printf("Decode...\n");
    out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FARME_SIZE);
    int ret;
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == index) {
            ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0) {
                printf("avcodec_send_packet��%d\n", ret);
                continue;
            }
            while (ret >= 0) {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    //char* errbuf = (char*)av_malloc(100);
                    //size_t bug_size = 100;
                    //av_strerror(AVERROR(EAGAIN), errbuf, bug_size);
                    //printf("%s\n", errbuf);
                    //printf("avcodec_receive_frame 1��%d\n", ret);
                    break;
                }
                else if (ret < 0) {
                    printf("avcodec_receive_frame��%d\n", AVERROR(ret));
                    return -1;
                }

                if (ret >= 0) {   //AVFrame->Audio 
                    out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FARME_SIZE);
                    //�ز���
                    swr_convert(swrCtx, &out_buffer, MAX_AUDIO_FARME_SIZE, (const uint8_t**)pFrame->data, pFrame->nb_samples);
                    //��ȡ�ж�����Ч��������out_buffer���ڴ���
                    out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb, pFrame->nb_samples, out_sample_fmt, 1);
                    PTFRAME frame = new TFRAME;
                    frame->data = out_buffer;
                    frame->size = out_buffer_size;
                    frame->samplerate = out_sample_rate;
                    queueData.push(frame);  //��������ݴ������
                }
            }
        }
        av_packet_unref(packet);
    }

    printf("Decoding ended.\n");
}


int AudioPlayer::audio_play_thread() {
    OpenAL_init();   //��ʼ��OpenAL

    alGenSources(1, &m_source);
    if (alGetError() != AL_NO_ERROR) {
        printf("Error generating audio source.");
        return -1;
    }
    ALfloat SourcePos[] = { 0.0, 0.0, 0.0 };
    ALfloat SourceVel[] = { 0.0, 0.0, 0.0 };
    ALfloat ListenerPos[] = { 0.0, 0, 0 };
    ALfloat ListenerVel[] = { 0.0, 0.0, 0.0 };
    // first 3 elements are "at", second 3 are "up"
    ALfloat ListenerOri[] = { 0.0, 0.0, -1.0,  0.0, 1.0, 0.0 };
    //����Դ����
    alSourcef(m_source, AL_PITCH, 1.0);
    alSourcef(m_source, AL_GAIN, 1.0);
    alSourcefv(m_source, AL_POSITION, SourcePos);
    alSourcefv(m_source, AL_VELOCITY, SourceVel);
    alSourcef(m_source, AL_REFERENCE_DISTANCE, 50.0f);
    alSourcei(m_source, AL_LOOPING, AL_FALSE);
    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    alListener3f(AL_POSITION, 0, 0, 0);
    alGenBuffers(NUMBUFFERS, m_buffers); //����������

    printf("Play...\n");

    ALint processed1 = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed1);
    for (int i = 0; i < NUMBUFFERS; i++) {
        SoundCallback(m_buffers[i]);
    }
    Play();

    while (!queueData.empty()) {  //����Ϊ�պ�ֹͣ����
        ALint processed = 0;
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
        while (processed > 0) {
            ALuint bufferID = 0;
            alSourceUnqueueBuffers(m_source, 1, &bufferID);
            SoundCallback(bufferID);
            processed--;
        }
        Play();
    }

    alSourceStop(m_source);
    alSourcei(m_source, AL_BUFFER, 0);
    alDeleteBuffers(NUMBUFFERS, m_buffers);
    alDeleteSources(1, &m_source);

    printf("End.\n");



    return 0;
}


int AudioPlayer::destory() {
    av_frame_free(&pFrame);
    //av_free(out_buffer);
    swr_free(&swrCtx);


    ALCcontext* pCurContext = alcGetCurrentContext();
    ALCdevice* pCurDevice = alcGetContextsDevice(pCurContext);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(pCurContext);
    alcCloseDevice(pCurDevice);

    return 0;
}



int AudioPlayer::audio_playing() {
    m_pAudio = std::move(std::make_shared<std::thread>(&AudioPlayer::audio_play_thread, this));

    return 0;
}