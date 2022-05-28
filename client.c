#include <stdio.h>
#include <libsoup/soup.h>

/*
 * 一个基于LibSoup的Web Client例子
 * 参考：https://libsoup.org/libsoup-2.4/libsoup-client-howto.html
 * 编译命令：编译命令：cc -o client client.c `pkg-config --cflags --libs libsoup-2.4`
 */

void DoGet()
{
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1/get");
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
                                         "http://127.0.0.1/image");
    goffset count = soup_message_headers_get_content_length(msg->response_headers);
    if (count)
    {
        void *buffer = malloc (count);

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
        }
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

        printf ("geg image ok.\n");
        // clean up
        free (buffer);
        g_object_unref (stream);
    }
    g_object_unref (msg);
    g_object_unref (session);
}

void DoPost(const char *filename)
{
    GError *error = NULL;
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1/post");

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
    }
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
}

void DoMjpeg()
{
    GError *error = NULL;
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1/mjpeg");
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
        if (!stream_part)
            break;
        SoupMessageHeaders *headers = soup_multipart_input_stream_get_headers(multipart);
        goffset count = soup_message_headers_get_content_length(headers);
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

    // clean up
    g_object_unref (multipart);
    g_object_unref (stream);
    g_object_unref (msg);
    g_object_unref (session);
}

void WsMessage(SoupWebsocketConnection *connection,
               gint                     type,
               GBytes                  *message,
               gpointer                 user_data)
{
    if (type == SOUP_WEBSOCKET_DATA_TEXT)
        printf ("get a message: %s\n",
            g_bytes_get_data(message,
                             NULL));
    else if (type == SOUP_WEBSOCKET_DATA_BINARY)
        printf ("get a message\nn");
    soup_websocket_connection_send_text(connection,
                                        "Hello Websocket!");
}

void WsClose (SoupWebsocketConnection *connection,
              gpointer                 user_data)
{
    printf ("Websocket connection closed!\n");
    guint *timeout_id = (guint *)user_data;
    g_source_remove (*timeout_id);
    free (timeout_id);
    g_object_unref (connection);
    soup_websocket_connection_close(connection,
                                    SOUP_WEBSOCKET_CLOSE_NORMAL,
                                    NULL);
}

gboolean WsTimer (gpointer data)
{
    SoupWebsocketConnection *connection = (SoupWebsocketConnection *)data;
    soup_websocket_connection_send_text(connection,
                                        "Hello Websocket!");
    return TRUE;
}

void WsReady (GObject *object,
            GAsyncResult *result,
            gpointer user_data)
{
    GError *error = NULL;
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
        error = NULL;
    }
    g_signal_connect (connection,
                      "message",
                      G_CALLBACK (WsMessage),
                      connection);
    guint *timeout_id = malloc (sizeof (guint));
    *timeout_id = g_timeout_add (1000,
                                 WsTimer,
                                 connection);
    g_object_ref (connection);
    g_signal_connect (connection,
                      "closed",
                      G_CALLBACK (WsClose),
                      timeout_id);
}

/*
 * WebSocket连接必须通过GMainLoop控制下的异步方式进行连接
 */
void DoWs()
{
    GError *error = NULL;
    SoupSession *session = soup_session_new ();
    SoupMessage *msg = soup_message_new ("GET",
                                         "http://127.0.0.1/ws");
    soup_session_websocket_connect_async (session,
                                          msg,
                                          NULL,
                                          G_PRIORITY_DEFAULT,
                                          NULL,
                                          WsReady,
                                          NULL);
    GMainLoop *loop = g_main_loop_new(NULL,
                                      FALSE);
    g_main_loop_run (loop);
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
        DoMjpeg();
    }
    else if (strcmp (argv[1], "ws") == 0)
    {
        DoWs();
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
