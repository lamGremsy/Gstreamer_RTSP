#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/rtsp-server/rtsp-server.h>

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

static GstElement *pipeline;

typedef struct
{
  GstElement *generator_pipe;
  GstElement *vid_appsink;
  GstElement *vid_appsrc;
  GstElement *vid_appsink1;
  GstElement *vid_appsrc1;
} MyContext;

static void
need_data (GstElement * appsrc, guint unused, MyContext * ctx)
{
  GstSample *sample;
  GstFlowReturn ret;
  // g_print ("0\n");

  if (appsrc == ctx->vid_appsrc)
    sample = gst_app_sink_pull_sample (GST_APP_SINK (ctx->vid_appsink));

  if (sample) {
    GstBuffer *buffer = gst_sample_get_buffer (sample);
    GstSegment *seg = gst_sample_get_segment (sample);
    GstClockTime pts, dts;

    /* Convert the PTS/DTS to running time so they start from 0 */
    pts = GST_BUFFER_PTS (buffer);
    if (GST_CLOCK_TIME_IS_VALID (pts))
      pts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, pts);

    dts = GST_BUFFER_DTS (buffer);
    if (GST_CLOCK_TIME_IS_VALID (dts))
      dts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, dts);

    if (buffer) {
      /* Make writable so we can adjust the timestamps */
      buffer = gst_buffer_copy (buffer);
      GST_BUFFER_PTS (buffer) = pts;
      GST_BUFFER_DTS (buffer) = dts;
      g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    }

    /* we don't need the appsink sample anymore */
    gst_sample_unref (sample);
  }
}

static void
need_data1 (GstElement * appsrc, guint unused, MyContext * ctx)
{
  GstSample *sample;
  GstFlowReturn ret;
  // g_print ("1\n");
  if (appsrc == ctx->vid_appsrc1)
    sample = gst_app_sink_pull_sample (GST_APP_SINK (ctx->vid_appsink1));

  if (sample) {
    GstBuffer *buffer = gst_sample_get_buffer (sample);
    GstSegment *seg = gst_sample_get_segment (sample);
    GstClockTime pts, dts;

    /* Convert the PTS/DTS to running time so they start from 0 */
    pts = GST_BUFFER_PTS (buffer);
    if (GST_CLOCK_TIME_IS_VALID (pts))
      pts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, pts);

    dts = GST_BUFFER_DTS (buffer);
    if (GST_CLOCK_TIME_IS_VALID (dts))
      dts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, dts);

    if (buffer) {
      /* Make writable so we can adjust the timestamps */
      buffer = gst_buffer_copy (buffer);
      GST_BUFFER_PTS (buffer) = pts;
      GST_BUFFER_DTS (buffer) = dts;
      g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    }

    /* we don't need the appsink sample anymore */
    gst_sample_unref (sample);
  }
}

static void
ctx_free (MyContext * ctx)
{
  gst_element_set_state (ctx->generator_pipe, GST_STATE_NULL);
  g_print ("NULL\n");
  gst_object_unref (ctx->generator_pipe);
  gst_object_unref (ctx->vid_appsrc );
  gst_object_unref (ctx->vid_appsink );
  g_free (ctx);
}

static void
ctx_free1 (MyContext * ctx)
{
  gst_element_set_state (ctx->generator_pipe, GST_STATE_NULL);
  g_print ("NULL\n");
  gst_object_unref (ctx->generator_pipe);
  gst_object_unref (ctx->vid_appsrc1 );
  gst_object_unref (ctx->vid_appsink1 );
  g_free (ctx);
}

static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc, *appsink;
    GstCaps *caps, *caps1;
    MyContext *ctx;

    g_print ("URL 0\n");

    // gst_element_set_state (pipeline, GST_STATE_PAUSED);

    ctx = g_new0 (MyContext, 1);

    ctx->generator_pipe = gst_parse_launch ("v4l2src is-live=1 device=/dev/video0  \
                                            ! videoconvert ! x264enc speed-preset=superfast tune=4 \
                                            ! appsink name=vid max-buffers=0 drop=true", NULL);

    // ctx->generator_pipe = gst_parse_launch ("videotestsrc is-live=true ! x264enc speed-preset=superfast tune=zerolatency \
    //                                         ! h264parse ! appsink name=vid max-buffers=1 drop=true ", NULL);

    /* make sure the data is freed when the media is gone */
    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free);

    element = gst_rtsp_media_get_element (media);

    caps = gst_caps_new_simple ("video/x-h264",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480,
                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);


    ctx->vid_appsrc  = appsrc  = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc");
    ctx->vid_appsink = appsink = gst_bin_get_by_name (GST_BIN (ctx->generator_pipe), "vid");
    // gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set (G_OBJECT (appsrc), "caps", caps, NULL);
    g_object_set (G_OBJECT (appsink), "caps", caps, NULL);
    
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_caps_unref (caps);
    // if (gst_element_get_state (ctx->generator_pipe) != GST_STATE_PLAYING)
    gst_element_set_state (ctx->generator_pipe, GST_STATE_PLAYING);
    gst_object_unref (element);
}

static void
media_configure1 (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc, *appsink;
    GstCaps *caps, *caps1;
    MyContext *ctx;

    g_print ("URL 1\n");
    // gst_element_set_state (pipeline, GST_STATE_PAUSED);
    ctx = g_new0 (MyContext, 1);

    ctx->generator_pipe = gst_parse_launch ("v4l2src is-live=1 device=/dev/video0  \
                                            ! videoconvert ! x264enc speed-preset=superfast tune=4 \
                                            ! appsink name=vid1 max-buffers=0 drop=true", NULL);

    // ctx->generator_pipe = gst_parse_launch ("videotestsrc is-live=true ! x264enc speed-preset=superfast tune=zerolatency \
    //                                         ! h264parse ! appsink name=vid1 max-buffers=1 drop=true ", NULL);

    /* make sure the data is freed when the media is gone */
    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free1);
    element = gst_rtsp_media_get_element (media);

    caps = gst_caps_new_simple ("video/x-h264",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 240,
                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    

    ctx->vid_appsrc1  = appsrc  = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc1");
    ctx->vid_appsink1 = appsink = gst_bin_get_by_name (GST_BIN (ctx->generator_pipe), "vid1");
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set (G_OBJECT (appsrc), "caps", caps, NULL);
    g_object_set (G_OBJECT (appsink), "caps", caps, NULL);
    
    g_signal_connect (appsrc, "need-data", (GCallback) need_data1, ctx);
    gst_caps_unref (caps);

    gst_element_set_state (ctx->generator_pipe, GST_STATE_PLAYING);
    gst_object_unref (element);
}

int main (int argc, char *argv[])
{
    GMainLoop             *loop;
    GstRTSPServer         *server;
    GstRTSPMountPoints    *mounts, *mounts1;
    GstRTSPMediaFactory   *factory, *factory1;

    gst_init (&argc, &argv);
    loop    = g_main_loop_new (NULL, FALSE);
    server  = gst_rtsp_server_new ();
    g_object_set(server, "service", "5004", NULL);
    
    mounts  = gst_rtsp_server_get_mount_points (server);
    // mounts1 = gst_rtsp_server_get_mount_points (server);
    factory = gst_rtsp_media_factory_new ();
    factory1= gst_rtsp_media_factory_new ();

    // pipeline = gst_parse_launch ("v4l2src is-live=1 device=/dev/video0  \
    //                             ! videoconvert ! x264enc speed-preset=superfast tune=zerolatency \
    //                             ! appsink name=vid emit-signals=1 max-buffer=1 drop=true ", NULL);

    // pipeline = gst_parse_launch ("v4l2src is-live=1 device=/dev/video0  \
    //                             ! tee name=t \
    //                             t. ! queue ! videoconvert ! x264enc speed-preset=superfast tune=4 ! appsink name=vid max-buffers=1 drop=true \
    //                             t. ! queue ! videoconvert ! x264enc speed-preset=superfast tune=4 ! appsink name=vid1 max-buffers=1 drop=true ", NULL);

    // pipeline =
    //   gst_parse_launch
    //   ("videotestsrc is-live=true \
    //   ! tee name=t ! queue ! x264enc speed-preset=superfast tune=zerolatency ! h264parse ! appsink name=vid max-buffers=1 drop=true \
    //   t. ! queue ! x264enc speed-preset=superfast tune=zerolatency ! h264parse ! appsink name=vid1 max-buffers=1 drop=true ",
    //   NULL);

    // pipeline = gst_parse_launch ("v4l2src device=/dev/video0  \
    //                             ! tee name=t \
    //                             t. ! queue ! v4l2sink device=/dev/video3 \
    //                             t. ! queue ! v4l2sink device=/dev/video4 ", NULL);
    // gst_element_set_state (pipeline, GST_STATE_PLAYING);

    gst_rtsp_media_factory_set_launch (factory,
        "( appsrc name=videosrc ! h264parse ! rtph264pay name=pay0 pt=96 )");
    // gst_rtsp_media_factory_set_shared(factory, TRUE);
    g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);
    gst_rtsp_mount_points_add_factory (mounts, "/test", factory);
    

    gst_rtsp_media_factory_set_launch (factory1,
        "( appsrc name=videosrc1 ! h264parse ! rtph264pay name=pay0 pt=96 )");
    // gst_rtsp_media_factory_set_shared(factory1, TRUE);
    g_signal_connect (factory1, "media-configure", (GCallback) media_configure1, NULL);
    gst_rtsp_mount_points_add_factory (mounts, "/test1", factory1);
    g_object_unref (mounts);
    // g_object_unref (mounts1);
    gst_rtsp_server_attach (server, NULL);

    // gchar *urlString = gst_rtsp_url_get_request_uri (uri);
    // std::cout << urlString << std::endl;

    g_print ("stream ready at rtsp://127.0.0.1:5004/test\n");
      g_print ("gst-launch-1.0 rtspsrc location=rtsp://192.168.12.78:5004/test ! rtph264depay ! decodebin ! videoconvert ! autovideosink sync=0 async=0\n");
    g_main_loop_run (loop);
    return 0;
}