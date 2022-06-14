#include <stdio.h>
#include <libsoup/soup.h>
#include <opus.h>
#include <SDL2/SDL.h>

GAsyncQueue *queue = NULL;
OpusEncoder *encoder = NULL;
OpusDecoder *decoder = NULL;
SDL_AudioSpec spec;
SDL_AudioDeviceID playback_id;
SDL_AudioDeviceID capture_id;

void WsMessage(SoupWebsocketConnection *connection,
               gint                     type,
               GBytes                  *message,
               gpointer                 user_data)
{
    g_async_queue_lock (queue);
    if (g_async_queue_length_unlocked (queue) > 1000)
        g_bytes_unref (g_async_queue_pop_unlocked (queue));
    g_async_queue_push_unlocked (queue,
                                 message);
    g_bytes_ref (message);
    g_async_queue_unlock (queue);
}

void WsClose (SoupWebsocketConnection *connection,
              gpointer                 user_data)
{
    SDL_CloseAudioDevice(playback_id);
    puts ("关闭放音设备");
    SDL_CloseAudioDevice(capture_id);
    puts ("关闭采音设备");
    opus_encoder_destroy (encoder);
    opus_decoder_destroy (decoder);
    while (g_async_queue_length(queue))
        g_bytes_unref (g_async_queue_pop (queue));
    g_async_queue_unref (queue);
    g_object_unref (connection);
    SDL_Quit();
}

void PlayAudio (void  *userdata,
                Uint8 *stream,
                int    len)
{
    SDL_memset (stream, '\0', len);
    GBytes *buffer = NULL;
    g_async_queue_lock(queue);
    if (g_async_queue_length_unlocked (queue))
        buffer = g_async_queue_pop_unlocked (queue);
    g_async_queue_unlock(queue);
    if (buffer)
    {
        float *pcm = (float *)malloc(len);
    
        int size = opus_decode_float (decoder,
                                      g_bytes_get_data (buffer,
                                                        NULL),
                                      g_bytes_get_size (buffer),
                                      pcm,
                                      spec.samples,
                                      0);
        if (size > 0)
            SDL_memcpy (stream,
                        pcm,
                        len);

        else
        {
            fprintf (stderr,
                     "解码结果[%d]。\n",
                     size);
            fprintf (stderr,
                     " %d: Ok\n",
                     OPUS_OK);
            fprintf (stderr,
                     "%d: Bad arg\n",
                     OPUS_BAD_ARG);
            fprintf (stderr,
                     "%d: Buffer too small\n",
                     OPUS_BUFFER_TOO_SMALL);
            fprintf (stderr,
                     "%d: Internal error\n",
                     OPUS_INTERNAL_ERROR);
            fprintf (stderr,
                     "%d: Unimplemented\n",
                     OPUS_UNIMPLEMENTED);
            fprintf (stderr,
                     "%d: Invalid state\n",
                     OPUS_INVALID_STATE);
            fprintf (stderr,
                     "%d: Alloc fail\n",
                     OPUS_ALLOC_FAIL);
        }
        free (pcm);
        g_bytes_unref (buffer);
    }
}

void CaptAudio (void  *userdata,
                Uint8 *stream,
                int    len)
{
    SoupWebsocketConnection *connection = (SoupWebsocketConnection *)userdata;
    unsigned char *encoded = malloc (len);
    gsize size = opus_encode_float (encoder,
                                    (float *)stream,
                                    spec.samples,
                                    encoded,
                                    len);
    if (size > 0)
    {
        if (SOUP_WEBSOCKET_STATE_OPEN == soup_websocket_connection_get_state (connection)
/*
 * 如果没有采集到样本数据，则编码后的数据长度为8
 * 这里的判断表示没有采集到数据时，则不进行数据发送，这样可以节省一些带宽       
 * 该值会随着freq、sample的值而变化，具体需要测试来确定
 */
        && size > 8)
        {
            soup_websocket_connection_send_binary (connection,
                                                   encoded,
                                                   size);
        }
    }
    else
    {
        fprintf (stderr,
                 "解码结果[%d]。\n",
                 (int)size);
        fprintf (stderr,
                 " %d: Ok\n",
                 OPUS_OK);
        fprintf (stderr,
                 "%d: Bad arg\n",
                 OPUS_BAD_ARG);
        fprintf (stderr,
                 "%d: Buffer too small\n",
                 OPUS_BUFFER_TOO_SMALL);
        fprintf (stderr,
                 "%d: Internal error\n",
                 OPUS_INTERNAL_ERROR);
        fprintf (stderr,
                 "%d: Unimplemented\n",
                 OPUS_UNIMPLEMENTED);
        fprintf (stderr,
                 "%d: Invalid state\n",
                 OPUS_INVALID_STATE);
        fprintf (stderr,
                 "%d: Alloc fail\n",
                 OPUS_ALLOC_FAIL);
    }
    free (encoded);
}

void ConnectionInit (SoupWebsocketConnection *connection,
                     const char *playback_device,
                     const char *capture_device)
{
    SDL_Init (SDL_INIT_AUDIO);
    SDL_zero(spec);
    spec.freq = 16000;
    spec.format = AUDIO_F32SYS;
    spec.channels = 1;
    spec.samples = spec.freq / 1000 * 20;
    spec.callback = PlayAudio;

    playback_id = SDL_OpenAudioDevice(playback_device,
                                      FALSE,
                                     &spec,
                                      NULL,
                                      0);
    if (!playback_id)
    {
        fprintf (stderr,
                 "无法打开放音设备[%s]：%s\n",
                 playback_device,
                 SDL_GetError());
        goto err_open_playback;
    }

    spec.callback = CaptAudio;
    spec.userdata = connection;

    capture_id = SDL_OpenAudioDevice (capture_device,
                                      SDL_TRUE,
                                     &spec,
                                      NULL,
                                      0);
    if (!capture_id)
    {
        fprintf (stderr,
                 "无法打开采音设备[%s]：%s\n",
                 capture_device,
                 SDL_GetError());
        goto err_open_capture;
    }

    encoder = opus_encoder_create(spec.freq,
                                  spec.channels,
                                  OPUS_APPLICATION_VOIP,
                                  NULL);
    decoder = opus_decoder_create(spec.freq,
                                  spec.channels,
                                  NULL);

    g_signal_connect (connection,
                      "message",
                      G_CALLBACK (WsMessage),
                      connection);
    g_signal_connect (connection,
                      "closed",
                      G_CALLBACK (WsClose),
                      NULL);
    queue = g_async_queue_new();
    SDL_PauseAudioDevice (playback_id,
                          SDL_FALSE);
    puts("打开放音设备。");
    SDL_PauseAudioDevice(capture_id,
                         SDL_FALSE);
    puts("打开采音设备。");
    g_object_ref (connection);

    return;
err_open_capture:
    SDL_CloseAudioDevice(playback_id);
err_open_playback:
err_connection:
    return;
}
