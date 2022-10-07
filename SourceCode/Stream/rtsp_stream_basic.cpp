#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/rtsp-server/rtsp-server.h>

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
  gst_object_unref (ctx->generator_pipe);
  gst_object_unref (ctx->vid_appsrc);
  gst_object_unref (ctx->vid_appsink);
  g_free (ctx);
}

static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc, *appsink;
    MyContext *ctx;

    ctx = g_new0 (MyContext, 1);

    ctx->generator_pipe = gst_parse_launch ("v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480,framerate=30/1 \
                                            ! videoconvert ! x264enc speed-preset=superfast tune=4 ! appsink name=vid drop=true", NULL);

    /* make sure the data is freed when the media is gone */
    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free);

    element = gst_rtsp_media_get_element (media);

    ctx->vid_appsrc = appsrc =
        gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc");
    ctx->vid_appsink = appsink =
        gst_bin_get_by_name (GST_BIN (ctx->generator_pipe), "vid");
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);

    gst_element_set_state (ctx->generator_pipe, GST_STATE_PLAYING);
    gst_object_unref (element);
}

int main (int argc, char *argv[])
{
  GMainLoop             *loop;
  GstRTSPServer         *server;
  GstRTSPMountPoints    *mounts;
  GstRTSPMediaFactory   *factory;

  gst_init (&argc, &argv);
  loop    = g_main_loop_new (NULL, FALSE);
  server  = gst_rtsp_server_new ();
  g_object_set(server, "service", "5004", NULL);
  mounts  = gst_rtsp_server_get_mount_points (server);
  factory = gst_rtsp_media_factory_new ();

  gst_rtsp_media_factory_set_launch (factory,
      "( appsrc name=videosrc is-live=true ! h264parse ! rtph264pay name=pay0 pt=97 sync=0 )");

  g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);

  gst_rtsp_mount_points_add_factory (mounts, "/test", factory);
  g_object_unref (mounts);
  gst_rtsp_server_attach (server, NULL);

  g_print ("stream ready at rtsp://127.0.0.1:8554/test\n");
  // g_print ("gst-launch-1.0 rtspsrc location=rtsp://192.168.12.78:8554/test ! rtph264depay ! decodebin ! videoconvert ! autovideosink sync=0 async=0\n");
  g_main_loop_run (loop);
  return 0;
}