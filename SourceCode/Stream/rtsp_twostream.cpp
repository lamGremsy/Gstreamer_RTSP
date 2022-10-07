#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/rtsp-server/rtsp-server.h>

static GstElement *pipeline , *tee;
static GstElement *queue_640, *videoconvert_640, *videoscale_640, *filter_640, *x264enc_640, *appsink_640;
static GstElement *queue_320, *videoconvert_320, *videoscale_320, *filter_320, *x264enc_320, *appsink_320;
static GstCaps    *caps_640 , *caps_320;

typedef struct
{
  GstElement *generator_pipe;
  GstElement *vid_appsink;
  GstElement *vid_appsrc;
} MyContext;

static void
need_data (GstElement * appsrc, guint unused, MyContext * ctx)
{
  GstSample *sample;
  GstFlowReturn ret;

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
      buffer = gst_buffer_copy (buffer);
      GST_BUFFER_PTS (buffer) = pts;
      GST_BUFFER_DTS (buffer) = dts;
      g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    }
    gst_sample_unref (sample);
  }
}

static void
ctx_free (MyContext * ctx)
{
  g_print ("Off stream video 640\n");
  gst_element_unlink_many(tee, queue_640, videoconvert_640, videoscale_640, filter_640, x264enc_640, appsink_640, NULL);

  gst_bin_remove(GST_BIN (pipeline), queue_640);
  gst_bin_remove(GST_BIN (pipeline), videoconvert_640);
  gst_bin_remove(GST_BIN (pipeline), videoscale_640);
  gst_bin_remove(GST_BIN (pipeline), filter_640);
  gst_bin_remove(GST_BIN (pipeline), x264enc_640);
  gst_bin_remove(GST_BIN (pipeline), appsink_640);

  gst_element_set_state(GST_ELEMENT(queue_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoconvert_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoscale_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(filter_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(x264enc_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(appsink_640), GST_STATE_NULL);

  gst_object_unref(GST_OBJECT(queue_640));
  gst_object_unref(GST_OBJECT(videoconvert_640));
  gst_object_unref(GST_OBJECT(videoscale_640));
  gst_object_unref(GST_OBJECT(filter_640));
  gst_object_unref(GST_OBJECT(x264enc_640));
  gst_object_unref(GST_OBJECT(appsink_640));

  gst_object_unref (ctx->vid_appsrc );
  gst_object_unref (ctx->vid_appsink );
  g_free (ctx);
}

static void
ctx_free1 (MyContext * ctx)
{
  g_print ("Off stream video 320\n");

  gst_element_unlink_many(tee, queue_320, videoconvert_320, videoscale_320, filter_320, x264enc_320, appsink_320, NULL);

  gst_bin_remove(GST_BIN (pipeline), queue_320);
  gst_bin_remove(GST_BIN (pipeline), videoconvert_320);
  gst_bin_remove(GST_BIN (pipeline), videoscale_320);
  gst_bin_remove(GST_BIN (pipeline), filter_320);
  gst_bin_remove(GST_BIN (pipeline), x264enc_320);
  gst_bin_remove(GST_BIN (pipeline), appsink_320);

  gst_element_set_state(GST_ELEMENT(queue_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoconvert_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoscale_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(filter_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(x264enc_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(appsink_320), GST_STATE_NULL);

  gst_object_unref(GST_OBJECT(queue_320));
  gst_object_unref(GST_OBJECT(videoconvert_320));
  gst_object_unref(GST_OBJECT(videoscale_320));
  gst_object_unref(GST_OBJECT(filter_320));
  gst_object_unref(GST_OBJECT(x264enc_320));
  gst_object_unref(GST_OBJECT(appsink_320));


  gst_object_unref (ctx->vid_appsrc );
  gst_object_unref (ctx->vid_appsink );
  g_free (ctx);
}

static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc;
    GstCaps *caps, *caps1;
    MyContext *ctx;

    g_print ("On stream video 640\n");
    ctx = g_new0 (MyContext, 1);

    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free);

    queue_640         = gst_element_factory_make("queue", NULL);
    videoconvert_640  = gst_element_factory_make("videoconvert", NULL);
    videoscale_640    = gst_element_factory_make("videoscale", NULL);
    filter_640        = gst_element_factory_make("capsfilter",NULL);
    x264enc_640       = gst_element_factory_make("x264enc", NULL);
    ctx->vid_appsink  = appsink_640 = gst_element_factory_make("appsink", "vid");
    
    caps_640 = gst_caps_from_string("video/x-raw,width=640,height=480,framerate=30/1");

    g_object_set (G_OBJECT(filter_640), "caps", caps_640, NULL);
    g_object_set (x264enc_640, "tune", 4, NULL);
    g_object_set (x264enc_640, "speed-preset", 2, NULL);
    g_object_set (appsink_640, "drop", 1, NULL);

    gst_element_set_state (pipeline, GST_STATE_READY);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);

    gst_bin_add_many (GST_BIN (pipeline),
                      queue_640, videoconvert_640, videoscale_640, filter_640, x264enc_640, appsink_640, NULL);
    gst_element_link_many(tee, queue_640, videoconvert_640, videoscale_640, filter_640, x264enc_640, appsink_640, NULL);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    element = gst_rtsp_media_get_element (media);

    caps = gst_caps_new_simple ("video/x-h264",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480,
                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

    ctx->vid_appsrc  = appsrc  = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc");
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set (G_OBJECT (appsink_640), "caps", caps, NULL);
    g_object_set (G_OBJECT (appsrc), "caps", caps, NULL);
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_caps_unref (caps);
    gst_object_unref (element);
}

static void
media_configure1 (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc;
    GstCaps *caps;
    MyContext *ctx;

    g_print ("On stream video 320\n");
    ctx = g_new0 (MyContext, 1);

    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free1);

    queue_320         = gst_element_factory_make("queue", NULL);
    videoconvert_320  = gst_element_factory_make("videoconvert", NULL);
    videoscale_320    = gst_element_factory_make("videoscale", NULL);
    filter_320        = gst_element_factory_make("capsfilter",NULL);
    x264enc_320       = gst_element_factory_make("x264enc", NULL);
    ctx->vid_appsink  = appsink_320 = gst_element_factory_make("appsink", "vid1");
    
    caps_320 = gst_caps_from_string("video/x-raw,width=320,height=240,framerate=30/1");
    g_object_set (G_OBJECT(filter_320), "caps", caps_320, NULL);
    g_object_set (x264enc_320, "tune", 4, NULL);
    g_object_set (x264enc_320, "speed-preset", 2, NULL);
    g_object_set (appsink_320, "drop", 1, NULL);

    gst_element_set_state (pipeline, GST_STATE_READY);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);

    gst_bin_add_many (GST_BIN (pipeline),
                      queue_320, videoconvert_320, videoscale_320, filter_320, x264enc_320, appsink_320, NULL);
    gst_element_link_many (tee, queue_320, videoconvert_320, videoscale_320, filter_320, x264enc_320, appsink_320, NULL);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    element = gst_rtsp_media_get_element (media);

    caps = gst_caps_new_simple ("video/x-h264",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 240,
                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    
    ctx->vid_appsrc  = appsrc  = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc1");
    // ctx->vid_appsink = appsink = gst_bin_get_by_name (GST_BIN (pipeline), "vid1");
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set (G_OBJECT (appsrc), "caps", caps, NULL);
    g_object_set (G_OBJECT (appsink_320), "caps", caps, NULL);
    
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_caps_unref (caps);
    gst_object_unref (element);
}

int main (int argc, char *argv[])
{
    GMainLoop             *loop;
    GstRTSPServer         *server;
    GstRTSPMountPoints    *mounts;
    GstRTSPMediaFactory   *factory, *factory1;

    gst_init (&argc, &argv);
    loop    = g_main_loop_new (NULL, FALSE);
    server  = gst_rtsp_server_new ();
    g_object_set(server, "service", "5004", NULL);

    mounts  = gst_rtsp_server_get_mount_points (server);
    factory = gst_rtsp_media_factory_new ();
    factory1= gst_rtsp_media_factory_new ();

    pipeline = gst_parse_launch ("v4l2src device=/dev/video0 name=source ! video/x-raw,width=640,height=480,framerate=30/1 \
                                ! tee name=t t. ! queue ! videoconvert ! fakesink " , NULL);

    tee = gst_bin_get_by_name (GST_BIN (pipeline), "t");    

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    gst_rtsp_media_factory_set_launch (factory,
        "( appsrc name=videosrc is-live=true ! h264parse ! rtph264pay name=pay0 pt=96 )");
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);
    gst_rtsp_mount_points_add_factory (mounts, "/test", factory);

    gst_rtsp_media_factory_set_launch (factory1,
        "( appsrc name=videosrc1 is-live=true ! h264parse ! rtph264pay name=pay0 pt=96 )");
    gst_rtsp_media_factory_set_shared(factory1, TRUE);
    g_signal_connect (factory1, "media-configure", (GCallback) media_configure1, NULL);
    gst_rtsp_mount_points_add_factory (mounts, "/test1", factory1);

    g_object_unref (mounts);
    gst_rtsp_server_attach (server, NULL);
    
    g_print ( "Stream ready at rtsp://127.0.0.1:5004/test  -- Stream video 640x480\n"
              "Stream ready at rtsp://127.0.0.1:5004/test1 -- Stream video 320x240\n\n");
    g_main_loop_run (loop);
    return 0;
}