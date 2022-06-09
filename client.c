#include <stdio.h>
#include <libsoup/soup.h>
#include <opus.h>
#include <SDL2/SDL.h>
#include "ws_util.h"

/*
 * 一个基于LibSoup的Web Client例子
 * 参考：https://libsoup.org/libsoup-2.4/libsoup-client-howto.html
 * 编译命令：编译命令：cc -o client client.c `pkg-config --cflags --libs libsoup-2.4`
 */

void DoGet()
{
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1:1080/get");
    guint code = soup_session_send_message (session,
                                            msg);
    printf ("response status code: %d\n",
            code);

    // clean up
    g_object_unref (msg);
    g_object_unref (session);
}

void DoImage()
{
    GError *error = NULL;
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1:1080/image");
    GInputStream *stream = soup_session_send(session,
                                             msg,
                                             NULL,
                                            &error);
    if (error)
    {
        fprintf (stderr,
                 "send request error: %s\n",
                 error->message);
        g_error_free (error);
        error = NULL;
        goto err_send;
    }
    goffset count = soup_message_headers_get_content_length(msg->response_headers);
    if (count)
    {
        void *buffer = malloc (count);
        g_input_stream_read_all (stream,
                                 buffer,
                                 count,
                                 NULL,
                                 NULL,
                                &error);
        if (error)
        {
            fprintf (stderr,
                     "read response error: %s\n",
                     error->message);
            g_error_free (error);
            error = NULL;
        }

        printf ("get image ok.\n");
        // clean up
        free (buffer);
        g_object_unref (stream);
    }
err_send:
    g_object_unref (msg);
    g_object_unref (session);
}

void DoPost(const char *filename)
{
    GError *error = NULL;
    gchar         *body  = NULL;
    gsize          length;
    g_file_get_contents (filename,
                         &body,
                         &length,
                         &error);

    if (error)
    {
        fprintf (stderr,
                 "Can't read from file: %s\n",
                 filename);
        g_error_free (error);
        error = NULL;
        goto err_file;
    }

    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1:1080/post");
    soup_message_set_request (msg,
                              "image/jpeg",
                              SOUP_MEMORY_TAKE,
                              body,
                              length);
    guint code = soup_session_send_message (session,
                                            msg);
    printf ("response status code: %d\n",
            code);

    // clean up
    g_object_unref (msg);
    g_object_unref (session);
err_file:
    return;
}

/*
 * 用同步方式实现
 */
void DoMjpeg ()
{
    GError *error = NULL;
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1:1080/mjpeg");
    GInputStream *stream = soup_session_send(session,
                                             msg,
                                             NULL,
                                            &error);
    if (error)
    {
        fprintf (stderr,
                 "send request error: %s\n",
                 error->message);
        g_error_free (error);
        error = NULL;
        goto err_send;
    }
    SoupMultipartInputStream *multipart = soup_multipart_input_stream_new (msg,
                                                                           stream);
    for (int i = 0; i < 10; i++)
    {
        GInputStream *stream_part = NULL;
        stream_part = soup_multipart_input_stream_next_part (multipart,
                                                             NULL,
                                                            &error);
        if (error)
        {
            fprintf (stderr,
                     "get next stream error: %s\n",
                     error->message);
            g_error_free (error);
            error = NULL;
        }
        goffset count = soup_message_headers_get_content_length(soup_multipart_input_stream_get_headers(multipart));
        if (count)
        {
            void *buffer = g_malloc (count);
            g_input_stream_read_all (stream,
                                     buffer,
                                     count,
                                     NULL,
                                     NULL,
                                    &error);
            if (error)
            {
                fprintf (stderr,
                         "read response error: %s\n",
                         error->message);
                g_error_free (error);
                error = NULL;
                goto err_read;
            }

            printf ("get image ok.\n");
            g_free (buffer);
        }
err_read:
        g_object_unref (stream_part);
    }

    // clean up
    g_object_unref (multipart);
    g_object_unref (stream);
err_send:
    g_object_unref (msg);
    g_object_unref (session);
}

typedef struct
{
    SoupSession *session;
    SoupMessage *msg;
    GMainLoop   *loop;
} MjpegInfo;

void PlayMjpeg (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
    MjpegInfo    *info   = (MjpegInfo *)user_data;           
    GError       *error  = NULL;
    GInputStream *stream = soup_session_send_finish (info->session,
                                                     result,
                                                    &error);
    if (error)
    {
        fprintf (stderr,
                 "read response error: %s\n",
                 error->message);
        g_error_free (error);
        error = NULL;
    }
    printf ("get reponse's stream\n");
    SoupMultipartInputStream *multipart = soup_multipart_input_stream_new (info->msg,
                                                                           stream);
    for (int i = 0; i < 10; i++)
    {
        GInputStream *stream_part = NULL;
        stream_part = soup_multipart_input_stream_next_part (multipart,
                                                             NULL,
                                                            &error);
        if (error)
        {
            fprintf (stderr,
                     "get next stream error: %s\n",
                     error->message);
            g_error_free (error);
            error = NULL;
        }
        goffset count = soup_message_headers_get_content_length(soup_multipart_input_stream_get_headers(multipart));
        if (count)
        {
            void *buffer = g_malloc (count);
            g_input_stream_read_all (stream,
                                     buffer,
                                     count,
                                     NULL,
                                     NULL,
                                    &error);
            if (error)
            {
                fprintf (stderr,
                         "read response error: %s\n",
                         error->message);
                g_error_free (error);
                error = NULL;
            }

            printf ("get image ok.\n");
            g_free (buffer);
        }
        g_object_unref (stream_part);
    }
    g_object_unref (multipart);
    g_object_unref (stream);
    g_main_loop_quit (info->loop);
    free (info);
}

/*
 * 用异步方式实现
 */
void DoMjpegAsync()
{
    MjpegInfo *info  = malloc (sizeof (MjpegInfo));
    GError    *error = NULL;
    info->session    = soup_session_new ();
    info->msg        = soup_message_new ("GET",
                                         "http://127.0.0.1:1080/mjpeg");
    soup_session_send_async (info->session,
                             info->msg,
                             NULL,
                             PlayMjpeg,
                             info);
    info->loop = g_main_loop_new (NULL,
                                  FALSE);
    g_main_loop_run(info->loop);
}

void WsReady (GObject *object,
            GAsyncResult *result,
            gpointer user_data)
{
    GError *error = NULL;
    WsInfo *info = (WsInfo *)user_data;
    SoupSession *session = (SoupSession *)object;
    SoupWebsocketConnection *connection = soup_session_websocket_connect_finish (session,
                                                                                 result,
                                                                                &error);
    if (error)
    {
        fprintf (stderr,
                 "get connection error: %s\n",
                 error->message);
        g_error_free (error);
    }
    ConnectionInit (connection,
                    info->playback_device,
                    info->capture_device);
    free (info);
}

/*
 * WebSocket连接必须通过GMainLoop控制下的异步方式进行连接
 */
void DoWs(const char *playback_device,
          const char *capture_device)
{
    SDL_Init (SDL_INIT_AUDIO);
    GError *error = NULL;
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1:1080/ws");
    WsInfo *info = malloc (sizeof (WsInfo));
    info->playback_device = playback_device;
    info->capture_device = capture_device;
    soup_session_websocket_connect_async (session,
                                          msg,
                                          NULL,
                                          NULL,
                                          NULL,
                                          WsReady,
                                          info);
    GMainLoop *loop = g_main_loop_new(NULL,
                                      FALSE);
    g_main_loop_run (loop);

    printf ("Quit!\n");
    SDL_Quit();
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf ("Usage: %s [ get | image | post | mjpeg | ws]\n",
                argv[0]); 
        return -1;
    } 
    if (strcmp (argv[1], "get") == 0)
    {
        DoGet();
    }
    else if (strcmp (argv[1], "image") == 0)
    {
        DoImage();
    }
    else if (strcmp (argv[1], "post") == 0)
    {
        if (argc == 3)
            DoPost(argv[2]);
        else
            printf ("Usage: %s %s <filename>\n",
                    argv[0],
                    argv[1]); 
        
    }
    else if (strcmp (argv[1], "mjpeg") == 0)
    {
        if (argc == 3
        && strcmp(argv[2], "a") == 0)
            DoMjpegAsync();
        else
            DoMjpeg();
    }
    else if (strcmp (argv[1], "ws") == 0)
    {
        if (argc == 4)
            DoWs(argv[2], argv[3]);
        else
        {
            printf ("Usage: %s %s <playback device> <capture device>\n",
                    argv[0],
                    argv[1]); 
            SDL_Init(SDL_INIT_AUDIO);
            puts ("录音设备:");
            for (int i = 0; i < SDL_GetNumAudioDevices(SDL_TRUE); ++i)
            {
                puts (SDL_GetAudioDeviceName(i,
                                             SDL_TRUE));
            }
            puts ("");
            puts ("播放设备:");
            for (int i = 0; i < SDL_GetNumAudioDevices(SDL_FALSE); ++i)
            {
                puts (SDL_GetAudioDeviceName(i,
                                             SDL_FALSE));
            }
            SDL_Quit();
        }
    }
    else
    {
        fprintf (stderr,
                 "not support: %s\n",
                 argv[1]);
        return -1;
    }
    return 0;
}
