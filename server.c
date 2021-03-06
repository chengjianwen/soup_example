#include <stdio.h>
#include <opus.h>
#include <SDL2/SDL.h>
#include <libsoup/soup.h>
#include "ws_util.h"

/*
 * 一个基于LibSoup的Web Server例子
 * 参考https://libsoup.org/libsoup-2.4/libsoup-server-howto.html
 * 提供如下的服务：
 *      /get   :
 *      /image : 返回一副图片
 *      /post  : 上传一副图片
 *      /mjpeg : 获取mjpeg视频
 *      /ws    : 建立websocket双向通道，每秒钟将当前时间发送给客户
 * 编译命令：cc -o server server.c `pkg-config --cflags --libs libsoup-2.4`
 */

/*
 * get服务，该服务仅仅向客户端返回一个连接正常的HTTP头。
 * 这是请求应答的最低要求。
 * 无需对任何资源(msg、path、query、client等)进行释放处理，Soup会自动释放它们。
 */
void GetHandler (SoupServer        *server,
                 SoupMessage       *msg,
                 char const        *path,
                 GHashTable        *query,
                 SoupClientContext *client,
                 gpointer          user_data)
{
    soup_message_set_status(msg, SOUP_STATUS_OK);
    printf ("a get request.\n");
}

/*
 * image服务，该服务向客户端返回一副图片。
 */
void ImageHandler (SoupServer        *server,
                   SoupMessage       *msg,
                   char const        *path,
                   GHashTable        *query,
                   SoupClientContext *client,
                   gpointer          user_data)
{
    soup_message_set_status(msg, SOUP_STATUS_OK);
    soup_message_headers_set_content_type(msg->response_headers,
                                          "image/jpeg",
                                           NULL);
    const char *filename = "example.jpg";
    gchar         *body  = NULL;
    gsize          length;
    GError        *error = NULL;
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
    }

    soup_message_set_response (msg,
                               "image/jpeg",
                               SOUP_MEMORY_TAKE,
	                       body,
                               length);
    // clean up
    printf ("image request.\n");
}

void PostHandler (SoupServer        *server,
                  SoupMessage       *msg,
                  char const        *path,
                  GHashTable        *query,
                  SoupClientContext *client,
                  gpointer          user_data)
{
    SoupMessageHeadersIter iter;
    soup_message_headers_iter_init (&iter,
                                    msg->request_headers);

    const char *name = NULL;
    const char *value = NULL;
    while (soup_message_headers_iter_next (&iter,
                                           &name,
                                           &value))
    {
        printf ("%s: %s\n",
                name,
                value);
    }

    soup_message_set_status(msg, SOUP_STATUS_OK);
    int pos_x = -1;
    int pos_y = -1;

    if ((value = soup_message_headers_get_one(msg->request_headers,
                                              "Pos-X")) != NULL)
    {
        pos_x = atoi(value);
        printf ("pos-x: %d\n",
                pos_x);
    }
    if ((value = soup_message_headers_get_one(msg->request_headers,
                                              "Pos-Y")) != NULL)
    {
        pos_y = atoi(value);
        printf ("pos-y: %d\n",
                pos_y);
    }
    const char    *filename = "post.jpg";
    GError        *error    = NULL;
    g_file_set_contents (filename,
                         msg->request_body->data,
                         msg->request_body->length,
                        &error);
    if (error)
    {
        fprintf (stderr,
                 "Can't write to file: %s\n",
                 filename);
        g_error_free (error);
        error = NULL;
    }
}

typedef struct
{
    SoupServer  *server;
    SoupMessage *msg;
    guint        timeout_id;
} SoupServerInfo;

gboolean Jpeger (gpointer data)
{
    SoupServerInfo *info = (SoupServerInfo *)data;
    const char *filename = "example.jpg";
    gchar      *body     = NULL;
    gsize       length;
    GError     *error    = NULL;
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
    }

    gchar *header = g_malloc (1024);
    struct timespec t;
    clock_gettime (CLOCK_REALTIME , &t);
    sprintf (header,
             "%s\nContent-Type: image/jpeg\nContent-Length: %ld\nX-Timestamp: %.6f\n\n",
             "\n--boundarydonotcross",
             length,
             t.tv_sec + (double)t.tv_nsec / 1000000000);
    soup_message_body_append (info->msg->response_body,
                              SOUP_MEMORY_TAKE,
                              header,
                              strlen(header));
    soup_message_body_append (info->msg->response_body,
                              SOUP_MEMORY_TAKE,
                              body,
                              length);
    soup_server_unpause_message (info->server,
                                 info->msg);
    printf ("send a jpeg file.\n");
    return TRUE;
}

void MjpegHandlerFinished (SoupMessage *msg,
                           gpointer     user_data)
{
    SoupServerInfo *info= (SoupServerInfo *)user_data;
    g_source_remove (info->timeout_id);
    free (info);
}

void MjpegHandler (SoupServer        *server,
                   SoupMessage       *msg,
                   char const        *path,
                   GHashTable        *query,
                   SoupClientContext *client,
                   gpointer          user_data)
{
    soup_message_set_status(msg, SOUP_STATUS_OK);
    soup_message_headers_set_encoding (msg->response_headers,
                                       SOUP_ENCODING_CHUNKED);
    soup_message_headers_set_content_type(msg->response_headers,
                                          "multipart/x-mixed-replace;boundary=boundarydonotcross",
                                           NULL);
    SoupServerInfo *info = g_malloc (sizeof (SoupServerInfo));
    info->server = server;
    info->msg = msg;
    info->timeout_id = g_timeout_add (1000,
                                      Jpeger,
                                      info);
    g_signal_connect (msg,
                      "finished",
                      G_CALLBACK (MjpegHandlerFinished),
                      info);
}

void WsHandler (SoupServer *server,
                SoupWebsocketConnection *connection,
                const char *path,
                SoupClientContext *client,
                gpointer user_data)
{
    WsInfo *info = (WsInfo *)user_data;
    ConnectionInit (connection,
                    info->playback_device,
                    info->capture_device);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf ("Usage: %s <放音设备> <采音设备>\n",
                argv[0]);
        SDL_Init(SDL_INIT_AUDIO);
        puts ("放音设备:");
        for (int i = 0; i < SDL_GetNumAudioDevices(SDL_TRUE); ++i)
        {
            puts(SDL_GetAudioDeviceName(i,
                                        SDL_FALSE));
        }
        puts ("");
        puts ("采音设备:");
        for (int i = 0; i < SDL_GetNumAudioDevices(SDL_FALSE); ++i)
        {
            puts(SDL_GetAudioDeviceName(i,
                                        SDL_TRUE));
        }
        SDL_Quit();
        goto err_usage;
    }
    SoupServer *server = soup_server_new(SOUP_SERVER_SERVER_HEADER,
                                         "Soup Example Server",
                                         NULL);

    if (!server)
    {
        fprintf (stderr,
                 "Error on SoupServer new.\n");
        goto err_server;
    }
    GError *error = NULL;
    soup_server_listen_all (server,
                            1080,
                            0,
                           &error);
    if (error)
    {
        fprintf (stderr,
                 "Error on SoupServer listen: %s\n",
                 error->message);
        g_error_free (error);
        error = NULL;
        goto err_listen;
    }
    soup_server_add_handler(server,
                            "/get",
                            GetHandler,
                            NULL,
                            NULL);
    soup_server_add_handler(server,
                            "/image",
                            ImageHandler,
                            NULL,
                            NULL);
    soup_server_add_handler(server,
                            "/post",
                            PostHandler,
                            NULL,
                            NULL);
    soup_server_add_handler(server,
                            "/mjpeg",
                            MjpegHandler,
                            NULL,
                            NULL);
/*
 * WsInfo的内容包括放音设备和采音设备
 */
    WsInfo *info = malloc (sizeof (WsInfo));
    info->playback_device = argv[1];
    info->capture_device = argv[2];
    soup_server_add_websocket_handler (server,
                                       "/ws",
                                       NULL,
                                       NULL,
                                       WsHandler,
                                       info,
                                       NULL);

    GMainLoop *loop =  g_main_loop_new(NULL,
                                       FALSE);
    g_main_loop_run(loop);

    // clean up
    soup_server_remove_handler (server,
                                "/ws");
    free (info);
    soup_server_remove_handler (server,
                                "/mjpeg");
    soup_server_remove_handler (server,
                                "/post");
    soup_server_remove_handler (server,
                                "/image");
    soup_server_remove_handler (server,
                                "/get");
    g_main_loop_unref(loop);

err_listen:
    g_object_unref(server);
err_server:
err_usage:
    return 0;
}
